cmake_minimum_required(VERSION 3.24)

project(
    CaptainWeb
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
    captain_sqlite
    STATIC
        third_party/sqlite/sqlite3.c
)

target_include_directories(
    captain_sqlite
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite
)

target_compile_definitions(
    captain_sqlite
    PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_DEFAULT_MEMSTATUS=0
)

add_executable(
    captain-web
        src/main.cpp
        src/storage/local_yaml_storage.cpp
        src/storage/sqlite_management_storage.cpp
        src/repository/hybrid_repository.cpp
        src/sync/sync_service.cpp
        src/api/api_server.cpp
)

target_include_directories(
    captain-web
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite
)

target_link_libraries(
    captain-web
    PRIVATE
        captain_sqlite
)

if(WIN32)
    target_compile_definitions(
        captain-web
        PRIVATE
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            NTDDI_VERSION=0x0A000000
    )

    target_link_libraries(
        captain-web
        PRIVATE
            ws2_32
            iphlpapi
    )
endif()

if(MSVC)
    target_compile_options(
        captain-web
        PRIVATE
            /W4
            /WX
            /permissive-
            /EHsc
    )
else()
    target_compile_options(
        captain-web
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Werror
    )

    target_compile_options(
        captain_sqlite
        PRIVATE
            -w
    )
endif()