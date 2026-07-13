cmake_minimum_required(VERSION 3.24)
project(CapComCLI VERSION 0.3.1 DESCRIPTION "Local CapCom command-line client" LANGUAGES CXX)
option(CAPCOM_BUILD_TESTS "Build CapCom CLI tests" ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(capcom_cli_core STATIC
    src/cli/command_parser.cpp
    src/commands/init_command.cpp
    src/commands/create_command.cpp
    src/config/app_config.cpp
    src/http/http_client.cpp
)
add_library(CapCom::CliCore ALIAS capcom_cli_core)
target_include_directories(capcom_cli_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(capcom_cli_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(capcom_cli_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(capcom_cli_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
add_executable(cap src/main.cpp)
target_link_libraries(cap PRIVATE CapCom::CliCore)
include(GNUInstallDirs)
install(TARGETS cap RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(CAPCOM_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
