from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from dotenv import load_dotenv

API_DIR = Path(__file__).resolve().parent
BACKEND_DIR = API_DIR.parent
PROJECT_ROOT = BACKEND_DIR.parent
load_dotenv(API_DIR / ".env")


def env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    normalized = raw.strip().lower()
    if normalized in {"1", "true", "yes", "on"}:
        return True
    if normalized in {"0", "false", "no", "off"}:
        return False
    raise RuntimeError(f"{name} must be true or false, got: {raw!r}")


@dataclass(frozen=True)
class Settings:
    api_key: str
    database_path: Path
    enforce_four_eyes: bool
    local_username: str
    local_role: str


def load_settings() -> Settings:
    api_key = os.getenv("CAPCOM_API_KEY", "").strip()
    if not api_key or api_key == "replace-with-a-long-random-key":
        raise RuntimeError(
            "Set CAPCOM_API_KEY in backend/api/.env to a strong local secret."
        )

    role = os.getenv("CAPCOM_LOCAL_ROLE", "developer").strip().lower()
    allowed_roles = {"admin", "developer", "reviewer", "auditor"}
    if role not in allowed_roles:
        raise RuntimeError(f"CAPCOM_LOCAL_ROLE must be one of {sorted(allowed_roles)}")

    configured_db = os.getenv("CAPCOM_DATABASE_PATH", "../db/traceability.db")
    database_path = Path(configured_db)
    if not database_path.is_absolute():
        database_path = (API_DIR / database_path).resolve()

    username = os.getenv("CAPCOM_LOCAL_USERNAME", "developer").strip()
    if not username:
        raise RuntimeError("CAPCOM_LOCAL_USERNAME must not be empty.")

    return Settings(
        api_key=api_key,
        database_path=database_path,
        enforce_four_eyes=env_bool("CAPCOM_ENFORCE_FOUR_EYES", False),
        local_username=username,
        local_role=role,
    )


settings = load_settings()
