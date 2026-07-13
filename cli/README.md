# CapCom CLI - Phase 3 Scaffold

This scaffold fixes the module boundaries before the command implementations:

- `cli`: argument parsing and command representation
- `config`: local project/API configuration
- `http`: FastAPI transport via libcurl
- `main.cpp`: composition root and exit-code handling

## Dependencies

- CMake 3.24+
- C++20 compiler
- Ninja
- vcpkg
- libcurl
- nlohmann-json

## Configure and build

Set `VCPKG_ROOT`, then run from the `cli` directory:

```bash
cmake --preset default
cmake --build --preset default
```

Run:

```bash
./build/debug/cap help
```

On Windows, the executable is `build\\debug\\cap.exe`.
