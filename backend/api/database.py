from __future__ import annotations

import json
import sqlite3
from contextlib import contextmanager
from pathlib import Path
from typing import Any, Iterator

from .settings import settings


def connect() -> sqlite3.Connection:
    """Open one request-scoped SQLite connection."""
    settings.database_path.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(
        settings.database_path,
        timeout=10.0,
        isolation_level=None,
        check_same_thread=False,
    )
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = ON;")
    connection.execute("PRAGMA busy_timeout = 10000;")
    connection.execute("PRAGMA journal_mode = WAL;")
    return connection


@contextmanager
def transaction(*, immediate: bool = False) -> Iterator[sqlite3.Connection]:
    connection = connect()
    try:
        connection.execute("BEGIN IMMEDIATE;" if immediate else "BEGIN;")
        yield connection
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


def row_to_dict(row: sqlite3.Row | None) -> dict[str, Any] | None:
    if row is None:
        return None
    result = dict(row)
    for field in ("attributes", "details"):
        value = result.get(field)
        if isinstance(value, str):
            try:
                result[field] = json.loads(value)
            except json.JSONDecodeError:
                pass
    return result


def initialize_local_user() -> None:
    """Create or update the identity represented by the local API key."""
    if not settings.database_path.is_file():
        raise RuntimeError(
            f"Database not found: {settings.database_path}. "
            "Run backend/db/init_db.py first."
        )

    with transaction(immediate=True) as connection:
        connection.execute(
            """
            INSERT INTO capcom_users (username, role)
            VALUES (?, ?)
            ON CONFLICT(username) DO UPDATE SET role = excluded.role
            """,
            (settings.local_username, settings.local_role),
        )
