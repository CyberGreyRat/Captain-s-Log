from __future__ import annotations

import json
import re
import sqlite3
from contextlib import asynccontextmanager
from typing import Annotated

from fastapi import Depends, FastAPI, HTTPException, Query, status

from .database import connect, initialize_local_user, row_to_dict, transaction
from .models import (
    AuditEntry,
    ItemCreate,
    ItemCreatedResponse,
    ItemResponse,
    ItemUpdate,
    LinkCreate,
    LinkResponse,
)
from .security import CurrentUser, require_api_key
from .settings import settings

UserDependency = Annotated[CurrentUser, Depends(require_api_key)]


@asynccontextmanager
async def lifespan(_: FastAPI):
    initialize_local_user()
    yield


app = FastAPI(
    title="CapCom API",
    version="0.2.0",
    description="Local ALM and traceability middleware.",
    lifespan=lifespan,
)


def get_item_by_uid(connection: sqlite3.Connection, uid: str) -> sqlite3.Row:
    row = connection.execute(
        "SELECT * FROM capcom_items WHERE uid = ?", (uid.upper(),)
    ).fetchone()
    if row is None:
        raise HTTPException(status_code=404, detail=f"Item {uid.upper()} not found.")
    return row


def next_uid(connection: sqlite3.Connection, item_type: str) -> str:
    # The IMMEDIATE transaction serializes writers, making MVP UID assignment safe.
    rows = connection.execute(
        "SELECT uid FROM capcom_items WHERE type = ? AND uid LIKE ?",
        (item_type, f"{item_type}-%"),
    ).fetchall()
    pattern = re.compile(rf"^{re.escape(item_type)}-(\d+)$")
    highest = 0
    for row in rows:
        match = pattern.fullmatch(row["uid"])
        if match:
            highest = max(highest, int(match.group(1)))
    return f"{item_type}-{highest + 1:03d}"


def would_create_cycle(
    connection: sqlite3.Connection, parent_id: int, child_id: int
) -> bool:
    if parent_id == child_id:
        return True
    # A cycle exists if the proposed parent is already reachable from the child.
    row = connection.execute(
        """
        WITH RECURSIVE descendants(id) AS (
            SELECT child_id FROM capcom_links WHERE parent_id = ?
            UNION
            SELECT links.child_id
              FROM capcom_links AS links
              JOIN descendants ON links.parent_id = descendants.id
        )
        SELECT 1 FROM descendants WHERE id = ? LIMIT 1
        """,
        (child_id, parent_id),
    ).fetchone()
    return row is not None


@app.get("/health")
def health() -> dict[str, str]:
    connection = connect()
    try:
        connection.execute("SELECT 1").fetchone()
    finally:
        connection.close()
    return {"status": "ok"}


@app.post(
    "/api/v1/items/create",
    response_model=ItemCreatedResponse,
    status_code=status.HTTP_201_CREATED,
)
def create_item(payload: ItemCreate, user: UserDependency) -> ItemCreatedResponse:
    try:
        with transaction(immediate=True) as connection:
            uid = next_uid(connection, payload.type)
            connection.execute(
                """
                INSERT INTO capcom_items
                    (uid, type, status, title, text, rationale, version,
                     git_hash, attributes, author_id)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    uid,
                    payload.type,
                    payload.status,
                    payload.title,
                    payload.text,
                    payload.rationale,
                    payload.version,
                    payload.git_hash,
                    json.dumps(payload.attributes, ensure_ascii=False),
                    user.id,
                ),
            )
    except sqlite3.IntegrityError as exc:
        raise HTTPException(status_code=409, detail="Item could not be created.") from exc

    return ItemCreatedResponse(
        uid=uid,
        status=payload.status,
        message="Item successfully created and logged to audit trail.",
    )


@app.get("/api/v1/items/{uid}", response_model=ItemResponse)
def read_item(uid: str, _: UserDependency) -> ItemResponse:
    connection = connect()
    try:
        item = row_to_dict(get_item_by_uid(connection, uid))
    finally:
        connection.close()
    return ItemResponse.model_validate(item)


@app.get("/api/v1/items", response_model=list[ItemResponse])
def list_items(
    _: UserDependency,
    item_type: str | None = Query(default=None, alias="type"),
    item_status: str | None = Query(default=None, alias="status"),
    limit: int = Query(default=100, ge=1, le=1000),
    offset: int = Query(default=0, ge=0),
) -> list[ItemResponse]:
    clauses: list[str] = []
    values: list[object] = []
    if item_type:
        clauses.append("type = ?")
        values.append(item_type.upper())
    if item_status:
        clauses.append("status = ?")
        values.append(item_status)
    where = f" WHERE {' AND '.join(clauses)}" if clauses else ""
    values.extend((limit, offset))

    connection = connect()
    try:
        rows = connection.execute(
            f"SELECT * FROM capcom_items{where} ORDER BY id LIMIT ? OFFSET ?",
            values,
        ).fetchall()
    finally:
        connection.close()
    return [ItemResponse.model_validate(row_to_dict(row)) for row in rows]


@app.patch("/api/v1/items/{uid}", response_model=ItemResponse)
def update_item(uid: str, payload: ItemUpdate, user: UserDependency) -> ItemResponse:
    changes = payload.model_dump(exclude_unset=True)
    if not changes:
        raise HTTPException(status_code=400, detail="No update fields supplied.")

    with transaction(immediate=True) as connection:
        current = get_item_by_uid(connection, uid)
        if changes.get("status") == "Approved" and settings.enforce_four_eyes:
            if user.role not in {"reviewer"}:
                raise HTTPException(status_code=403, detail="Reviewer role required.")
            if current["author_id"] == user.id:
                raise HTTPException(status_code=403, detail="Self-approval is not permitted.")

        if "attributes" in changes:
            changes["attributes"] = json.dumps(changes["attributes"], ensure_ascii=False)
        fields = list(changes)
        assignments = ", ".join(f"{field} = ?" for field in fields)
        connection.execute(
            f"UPDATE capcom_items SET {assignments} WHERE id = ?",
            [changes[field] for field in fields] + [current["id"]],
        )
        updated = row_to_dict(get_item_by_uid(connection, uid))
    return ItemResponse.model_validate(updated)


@app.post(
    "/api/v1/links/create",
    response_model=LinkResponse,
    status_code=status.HTTP_201_CREATED,
)
def create_link(payload: LinkCreate, _: UserDependency) -> LinkResponse:
    with transaction(immediate=True) as connection:
        parent = get_item_by_uid(connection, payload.parent_uid)
        child = get_item_by_uid(connection, payload.child_uid)
        if would_create_cycle(connection, parent["id"], child["id"]):
            raise HTTPException(status_code=409, detail="Link would create a graph cycle.")
        try:
            connection.execute(
                "INSERT INTO capcom_links (parent_id, child_id) VALUES (?, ?)",
                (parent["id"], child["id"]),
            )
        except sqlite3.IntegrityError as exc:
            raise HTTPException(status_code=409, detail="Link already exists.") from exc
    return LinkResponse(
        parent_uid=payload.parent_uid,
        child_uid=payload.child_uid,
        message="Traceability link successfully created.",
    )


@app.delete("/api/v1/links", response_model=LinkResponse)
def delete_link(payload: LinkCreate, _: UserDependency) -> LinkResponse:
    with transaction(immediate=True) as connection:
        parent = get_item_by_uid(connection, payload.parent_uid)
        child = get_item_by_uid(connection, payload.child_uid)
        cursor = connection.execute(
            "DELETE FROM capcom_links WHERE parent_id = ? AND child_id = ?",
            (parent["id"], child["id"]),
        )
        if cursor.rowcount == 0:
            raise HTTPException(status_code=404, detail="Link not found.")
    return LinkResponse(
        parent_uid=payload.parent_uid,
        child_uid=payload.child_uid,
        message="Traceability link successfully removed.",
    )


@app.get("/api/v1/audit", response_model=list[AuditEntry])
def list_audit_entries(
    _: UserDependency,
    item_uid: str | None = None,
    limit: int = Query(default=100, ge=1, le=1000),
    offset: int = Query(default=0, ge=0),
) -> list[AuditEntry]:
    connection = connect()
    try:
        if item_uid:
            rows = connection.execute(
                """
                SELECT * FROM capcom_audit_trail
                 WHERE item_uid = ? ORDER BY log_id DESC LIMIT ? OFFSET ?
                """,
                (item_uid.upper(), limit, offset),
            ).fetchall()
        else:
            rows = connection.execute(
                "SELECT * FROM capcom_audit_trail ORDER BY log_id DESC LIMIT ? OFFSET ?",
                (limit, offset),
            ).fetchall()
    finally:
        connection.close()
    return [AuditEntry.model_validate(row_to_dict(row)) for row in rows]
