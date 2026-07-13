# Integration

Copy `cli/` over your existing `cli/` directory.

Add to `add_library(capcom_cli_core STATIC ...)`:

```cmake
    src/yaml/yaml_store.cpp
    src/commands/yaml_commands.cpp
```

Extend `CommandType` in `command_parser.hpp`:

```cpp
scan, status, validate, test_import
```

In `command_parser.cpp`, add mappings:

```cpp
else if (name == "scan" || name == "-s") result.type = CommandType::scan;
else if (name == "status" || name == "-st") result.type = CommandType::status;
else if (name == "validate") result.type = CommandType::validate;
else if (name == "test" && argc >= 3 && std::string_view{argv[2]} == "import") {
    result.type = CommandType::test_import;
    for (int index = 3; index < argc; ++index) result.arguments.emplace_back(argv[index]);
    return result;
}
```

In `main.cpp`, include `capcom/commands/yaml_commands.hpp` and add cases:

```cpp
case CommandType::scan:
    return capcom::commands::ScanCommand{}.execute(std::filesystem::current_path());
case CommandType::status:
    return capcom::commands::StatusCommand{}.execute(std::filesystem::current_path());
case CommandType::validate:
    return capcom::commands::ValidateCommand{}.execute(std::filesystem::current_path());
case CommandType::test_import:
    if (command.arguments.size() != 1) throw std::runtime_error("Usage: cap test import <file>");
    return capcom::commands::TestImportCommand{}.execute(std::filesystem::current_path(), command.arguments[0]);
```

Commands:

```text
cap scan
cap status
cap validate
cap test import reports/test_results.xml
cap test import reports/test_results.json
```

`cap push` is intentionally not included yet. It needs the Backend YAML-UID upsert contract and an HTTP transport that works with the target Windows toolchain.
