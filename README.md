# Captain's Log v0.7.1: real `cap gui`

This patch updates both executables.

## Web server

- Direct URL receives Viewer access.
- `cap gui` uses a one-time ticket and an HttpOnly Developer session.
- Every POST route is protected server-side.
- The central catalog is `%USERPROFILE%\.captain\captain-central.db`.
- Starting `cap gui` registers the current project.
- GUI Push stores/upserts snapshots in the central catalog.
- The header project selector opens other projects as read-only snapshots.

## CLI

- Adds `CommandType::gui`.
- Adds parser and help text.
- Adds `GuiCommand`.
- Adds the source to CLI CMake.
- Rebuilds and copies the real `cap.exe` into `.venv\Scripts`.

## Install

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\apply_complete.ps1
.\build_and_install.ps1
```

Then reactivate the venv and check:

```powershell
cap help
cap gui
```
