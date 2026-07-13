# CapCom API – Phase 2

## Setup

Run Phase 1 first so that `backend/db/traceability.db` exists.

```bash
python -m venv .venv
```

Activate the environment:

```powershell
.venv\Scripts\Activate.ps1
```

Linux/macOS:

```bash
source .venv/bin/activate
```

Install and configure:

```bash
pip install -r backend/api/requirements.txt
```

Copy `backend/api/.env.example` to `backend/api/.env`. Generate a local key with:

```bash
python -c "import secrets; print(secrets.token_urlsafe(32))"
```

Insert that value as `CAPCOM_API_KEY`, then start from the project root:

```bash
uvicorn backend.api.main:app --reload --host 127.0.0.1 --port 8000
```

Swagger UI: <http://127.0.0.1:8000/docs>

For protected requests add the header `X-API-Key` with the value from `.env`.
