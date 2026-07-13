from __future__ import annotations

import secrets
import sqlite3
from dataclasses import dataclass

from fastapi import Header, HTTPException, status

from .database import connect
from .settings import settings


@dataclass(frozen=True)
class CurrentUser:
    id: int
    username: str
    role: str


def require_api_key(x_api_key: str | None = Header(default=None)) -> CurrentUser:
    if x_api_key is None or not secrets.compare_digest(x_api_key, settings.api_key):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Missing or invalid X-API-Key.",
        )

    connection = connect()
    try:
        row = connection.execute(
            "SELECT id, username, role FROM capcom_users WHERE username = ?",
            (settings.local_username,),
        ).fetchone()
    finally:
        connection.close()

    if row is None:
        raise HTTPException(status_code=500, detail="Local API user is not initialized.")
    return CurrentUser(id=row["id"], username=row["username"], role=row["role"])
