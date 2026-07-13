-- CapCom Phase 1 - relaxed local MVP schema
-- Strict approval and anti-delete rules are intentionally deferred to Phase 2+.

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS capcom_users (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    username    TEXT NOT NULL UNIQUE,
    role        TEXT NOT NULL DEFAULT 'developer'
                CHECK (role IN ('admin', 'developer', 'reviewer', 'auditor')),
    created_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS capcom_items (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    uid         TEXT NOT NULL UNIQUE,
    type        TEXT NOT NULL,
    status      TEXT NOT NULL DEFAULT 'Draft',
    title       TEXT NOT NULL,
    text        TEXT NOT NULL DEFAULT '',
    rationale   TEXT NOT NULL DEFAULT '',
    version     TEXT NOT NULL DEFAULT '0.1.0',
    git_hash    TEXT,
    attributes  TEXT NOT NULL DEFAULT '{}'
                CHECK (json_valid(attributes)),
    author_id   INTEGER,
    created_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (author_id) REFERENCES capcom_users(id)
        ON UPDATE CASCADE
        ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS capcom_links (
    parent_id   INTEGER NOT NULL,
    child_id    INTEGER NOT NULL,
    created_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (parent_id, child_id),
    CHECK (parent_id <> child_id),
    FOREIGN KEY (parent_id) REFERENCES capcom_items(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,
    FOREIGN KEY (child_id) REFERENCES capcom_items(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS capcom_audit_trail (
    log_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id     INTEGER,
    item_uid    TEXT,
    user_id     INTEGER,
    actor       TEXT NOT NULL DEFAULT 'LOCAL_USER',
    action      TEXT NOT NULL,
    details     TEXT NOT NULL DEFAULT '{}'
                CHECK (json_valid(details)),
    timestamp   TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (item_id) REFERENCES capcom_items(id)
        ON UPDATE CASCADE
        ON DELETE SET NULL,
    FOREIGN KEY (user_id) REFERENCES capcom_users(id)
        ON UPDATE CASCADE
        ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_capcom_items_type
    ON capcom_items(type);
CREATE INDEX IF NOT EXISTS idx_capcom_items_status
    ON capcom_items(status);
CREATE INDEX IF NOT EXISTS idx_capcom_items_author
    ON capcom_items(author_id);
CREATE INDEX IF NOT EXISTS idx_capcom_links_child
    ON capcom_links(child_id);
CREATE INDEX IF NOT EXISTS idx_capcom_audit_item_uid
    ON capcom_audit_trail(item_uid);
CREATE INDEX IF NOT EXISTS idx_capcom_audit_timestamp
    ON capcom_audit_trail(timestamp);

-- Keep updated_at current during ordinary MVP updates.
CREATE TRIGGER IF NOT EXISTS trg_capcom_items_touch_updated_at
AFTER UPDATE ON capcom_items
FOR EACH ROW
WHEN NEW.updated_at = OLD.updated_at
BEGIN
    UPDATE capcom_items
       SET updated_at = CURRENT_TIMESTAMP
     WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS trg_capcom_items_audit_insert
AFTER INSERT ON capcom_items
FOR EACH ROW
BEGIN
    INSERT INTO capcom_audit_trail (
        item_id, item_uid, user_id, actor, action, details
    ) VALUES (
        NEW.id,
        NEW.uid,
        NEW.author_id,
        COALESCE(
            (SELECT username FROM capcom_users WHERE id = NEW.author_id),
            'LOCAL_USER'
        ),
        'ITEM_CREATED',
        json_object(
            'new', json_object(
                'uid', NEW.uid,
                'type', NEW.type,
                'status', NEW.status,
                'title', NEW.title,
                'text', NEW.text,
                'rationale', NEW.rationale,
                'version', NEW.version,
                'git_hash', NEW.git_hash,
                'attributes', json(NEW.attributes),
                'author_id', NEW.author_id
            )
        )
    );
END;

CREATE TRIGGER IF NOT EXISTS trg_capcom_items_audit_update
AFTER UPDATE ON capcom_items
FOR EACH ROW
-- Ignore the internal timestamp-only update performed by the touch trigger.
WHEN OLD.uid IS NOT NEW.uid
  OR OLD.type IS NOT NEW.type
  OR OLD.status IS NOT NEW.status
  OR OLD.title IS NOT NEW.title
  OR OLD.text IS NOT NEW.text
  OR OLD.rationale IS NOT NEW.rationale
  OR OLD.version IS NOT NEW.version
  OR OLD.git_hash IS NOT NEW.git_hash
  OR OLD.attributes IS NOT NEW.attributes
  OR OLD.author_id IS NOT NEW.author_id
BEGIN
    INSERT INTO capcom_audit_trail (
        item_id, item_uid, user_id, actor, action, details
    ) VALUES (
        NEW.id,
        NEW.uid,
        NEW.author_id,
        COALESCE(
            (SELECT username FROM capcom_users WHERE id = NEW.author_id),
            'LOCAL_USER'
        ),
        'ITEM_UPDATED',
        json_object(
            'old', json_object(
                'uid', OLD.uid,
                'type', OLD.type,
                'status', OLD.status,
                'title', OLD.title,
                'text', OLD.text,
                'rationale', OLD.rationale,
                'version', OLD.version,
                'git_hash', OLD.git_hash,
                'attributes', json(OLD.attributes),
                'author_id', OLD.author_id
            ),
            'new', json_object(
                'uid', NEW.uid,
                'type', NEW.type,
                'status', NEW.status,
                'title', NEW.title,
                'text', NEW.text,
                'rationale', NEW.rationale,
                'version', NEW.version,
                'git_hash', NEW.git_hash,
                'attributes', json(NEW.attributes),
                'author_id', NEW.author_id
            )
        )
    );
END;

-- Deletion is allowed in the relaxed MVP, but remains visible in the audit log.
CREATE TRIGGER IF NOT EXISTS trg_capcom_items_audit_delete
BEFORE DELETE ON capcom_items
FOR EACH ROW
BEGIN
    INSERT INTO capcom_audit_trail (
        item_id, item_uid, user_id, actor, action, details
    ) VALUES (
        OLD.id,
        OLD.uid,
        OLD.author_id,
        COALESCE(
            (SELECT username FROM capcom_users WHERE id = OLD.author_id),
            'LOCAL_USER'
        ),
        'ITEM_DELETED',
        json_object(
            'old', json_object(
                'uid', OLD.uid,
                'type', OLD.type,
                'status', OLD.status,
                'title', OLD.title,
                'text', OLD.text,
                'rationale', OLD.rationale,
                'version', OLD.version,
                'git_hash', OLD.git_hash,
                'attributes', json(OLD.attributes),
                'author_id', OLD.author_id
            )
        )
    );
END;

CREATE TRIGGER IF NOT EXISTS trg_capcom_links_audit_insert
AFTER INSERT ON capcom_links
FOR EACH ROW
BEGIN
    INSERT INTO capcom_audit_trail (
        item_id, item_uid, actor, action, details
    ) VALUES (
        NEW.child_id,
        (SELECT uid FROM capcom_items WHERE id = NEW.child_id),
        'LOCAL_USER',
        'LINK_CREATED',
        json_object(
            'parent_uid', (SELECT uid FROM capcom_items WHERE id = NEW.parent_id),
            'child_uid',  (SELECT uid FROM capcom_items WHERE id = NEW.child_id)
        )
    );
END;

CREATE TRIGGER IF NOT EXISTS trg_capcom_links_audit_delete
BEFORE DELETE ON capcom_links
FOR EACH ROW
BEGIN
    INSERT INTO capcom_audit_trail (
        item_id, item_uid, actor, action, details
    ) VALUES (
        OLD.child_id,
        (SELECT uid FROM capcom_items WHERE id = OLD.child_id),
        'LOCAL_USER',
        'LINK_DELETED',
        json_object(
            'parent_uid', (SELECT uid FROM capcom_items WHERE id = OLD.parent_id),
            'child_uid',  (SELECT uid FROM capcom_items WHERE id = OLD.child_id)
        )
    );
END;

COMMIT;
