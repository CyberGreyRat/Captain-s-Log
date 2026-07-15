# Captain Web MVP

Hybride GUI/API für lokale YAML-Anforderungen und zentrale Managementdaten.

## Enthalten

- `LocalYamlStorage`: USR, SYS, SRS, SEC, ARCH und Tests als `.yml`
- `SqliteManagementStorage`: RISK, CLASS, CR, SOUP, Kommentare und Dateilinks
- `HybridRepository`: kombinierte Ansicht und typabhängiges Routing
- `SyncService`: zentrale Objekte als Read-only-YAML unter `reqs/remote/`
- REST-API mit cpp-httplib
- Tailwind-Dashboard mit Baum, Detailansicht und Modals

## Windows Build mit vcpkg

```powershell
vcpkg install
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
```

## Start

Argument 1 ist das Captain-Projekt, Argument 2 der Web-Ordner:

```powershell
.\build\Debug\captain-web.exe "C:\Projekte\MeinCaptainProjekt" ".\web"
```

Danach im Browser:

```text
http://127.0.0.1:8080
```

## API

- `GET /api/v1/requirements`
- `POST /api/v1/requirements/create`
- `GET/POST /api/v1/requirements/{uid}/comments|comment`
- `GET/POST /api/v1/requirements/{uid}/files|link_file`
- `POST /api/v1/sync/pull`

## Wichtige MVP-Grenzen

- SQLite simuliert die zentrale Management-Datenbank lokal. Später wird nur der Remote-Adapter gegen HTTP/SQL ausgetauscht.
- Authentifizierung und Rollen sind noch nicht enthalten.
- Parent/Child-Beziehungen werden im Create-Request gespeichert, aber die symmetrische Gegenstelle muss anschließend ergänzt werden.
- Datei-Anhänge sind Referenzen, keine Uploads.
- Für Produktion sind TLS, Auth, CSRF-Schutz, Revision/ETag und Audit-Signaturen erforderlich.
