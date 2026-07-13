# CapCom CLI 0.3.1

Build with the existing TDM-GCC preset:

```powershell
if (Test-Path build) { rmdir build -Recurse -Force }
cmake --preset default
cmake --build --preset default
```

Initialize and create an item from the CapCom project root:

```powershell
.\cli\build\debug\cap.exe init
.\cli\build\debug\cap.exe create srs "I2C Sensor auslesen"
```

`create` currently writes the local YAML source of truth. API synchronization remains isolated for the next step because libcurl is not yet linked under TDM-GCC.
