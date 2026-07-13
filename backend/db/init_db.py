#!/usr/bin/env python3
"""Initialize the local CapCom SQLite database for Phase 1."""

from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path


def initialize_database(db_path: Path, schema_path: Path) -> None:
    if not schema_path.is_file():
        raise FileNotFoundError(f"Schema file not found: {schema_path}")

    db_path.parent.mkdir(parents=True, exist_ok=True)
    schema_sql = schema_path.read_text(encoding="utf-8")

    connection = sqlite3.connect(db_path)
    try:
        connection.execute("PRAGMA foreign_keys = ON;")
        connection.executescript(schema_sql)

        foreign_keys_enabled = connection.execute(
            "PRAGMA foreign_keys;"
        ).fetchone()[0]
        if foreign_keys_enabled != 1:
            raise RuntimeError("SQLite foreign-key enforcement is disabled.")

        violations = connection.execute("PRAGMA foreign_key_check;").fetchall()
        if violations:
            raise RuntimeError(f"Foreign-key violations detected: {violations}")

        connection.commit()
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


def parse_args() -> argparse.Namespace:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(
        description="Create the local CapCom traceability database."
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=script_dir / "traceability.db",
        help="Output database path (default: backend/db/traceability.db)",
    )
    parser.add_argument(
        "--schema",
        type=Path,
        default=script_dir / "schema.sql",
        help="Schema SQL path (default: backend/db/schema.sql)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    initialize_database(args.db.resolve(), args.schema.resolve())
    print(f"CapCom database initialized: {args.db.resolve()}")


if __name__ == "__main__":
    main()
