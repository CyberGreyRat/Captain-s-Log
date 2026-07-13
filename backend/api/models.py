from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator

StatusName = Literal["Draft", "In Review", "Approved", "Rejected", "Failed", "Deprecated"]


class StrictModel(BaseModel):
    model_config = ConfigDict(extra="forbid")


class ItemCreate(StrictModel):
    type: str = Field(min_length=2, max_length=16, pattern=r"^[A-Za-z][A-Za-z0-9_]*$")
    title: str = Field(min_length=1, max_length=300)
    text: str = Field(default="", max_length=100_000)
    rationale: str = Field(default="", max_length=20_000)
    status: StatusName = "Draft"
    version: str = Field(default="0.1.0", min_length=1, max_length=50)
    git_hash: str | None = Field(default=None, max_length=64, pattern=r"^[0-9a-fA-F]+$")
    attributes: dict[str, Any] = Field(default_factory=dict)

    @field_validator("type")
    @classmethod
    def normalize_type(cls, value: str) -> str:
        return value.upper()


class ItemUpdate(StrictModel):
    title: str | None = Field(default=None, min_length=1, max_length=300)
    text: str | None = Field(default=None, max_length=100_000)
    rationale: str | None = Field(default=None, max_length=20_000)
    status: StatusName | None = None
    version: str | None = Field(default=None, min_length=1, max_length=50)
    git_hash: str | None = Field(default=None, max_length=64, pattern=r"^[0-9a-fA-F]+$")
    attributes: dict[str, Any] | None = None


class ItemResponse(StrictModel):
    id: int
    uid: str
    type: str
    status: str
    title: str
    text: str
    rationale: str
    version: str
    git_hash: str | None
    attributes: dict[str, Any]
    author_id: int | None
    created_at: str
    updated_at: str


class ItemCreatedResponse(StrictModel):
    success: bool = True
    uid: str
    status: str
    message: str


class LinkCreate(StrictModel):
    parent_uid: str = Field(min_length=3, max_length=40)
    child_uid: str = Field(min_length=3, max_length=40)

    @field_validator("parent_uid", "child_uid")
    @classmethod
    def normalize_uid(cls, value: str) -> str:
        return value.upper()


class LinkResponse(StrictModel):
    success: bool = True
    parent_uid: str
    child_uid: str
    message: str


class AuditEntry(StrictModel):
    log_id: int
    item_id: int | None
    item_uid: str | None
    user_id: int | None
    actor: str
    action: str
    details: dict[str, Any]
    timestamp: str
