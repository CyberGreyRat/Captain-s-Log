# CapCom CLI distribution

## Vorbereitung durch den Paket-Ersteller

Kopiere die fertig kompilierte Windows-Datei nach:

```text
src/capcom/bin/cap.exe
```

## Installation

Im Ordner `distribution`:

```powershell
pip install .
```

In einer aktivierten virtuellen Umgebung steht danach bereit:

```powershell
cap help
cap -c srs "Neue Anforderung"
cap -t
```

Für eine saubere Neuinstallation während der Entwicklung:

```powershell
pip uninstall capcom-cli -y
pip install --no-cache-dir --force-reinstall .
```

Dieses Paket ist derzeit ausschließlich für Windows vorgesehen.
