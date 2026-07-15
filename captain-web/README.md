# Captain Web v0.7 Complete

- direct browser session: Viewer, read-only;
- `cap gui`: one-time ticket and one-hour HttpOnly Developer session;
- all POST routes are protected server-side;
- central project catalog at `%USERPROFILE%\.captain\captain-central.db`;
- Push registers/upserts the active project and requirement snapshots;
- header project dropdown shows all registered projects;
- current `cap gui` project is editable, other projects are read-only snapshots;
- PowerShell profile wrapper makes `cap gui` available while forwarding all other commands to the existing `cap.exe`.

After applying, rebuild, close PowerShell, open a new PowerShell, activate the venv,
change into any initialized Captain project and run `cap gui`.
