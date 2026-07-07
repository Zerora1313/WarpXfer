@echo off
rem WarpXfer Direct Compiler Helper for Windows (Consolidated)

echo Building WarpXfer with GCC 16 (C++17 standard)...

g++ -std=c++17 -Wall -Wextra -Iinclude -o warp.exe ^
    src/main.cpp ^
    src/cli_parser.cpp ^
    src/transfer_manager.cpp ^
    src/sender_engine.cpp ^
    src/receiver_engine.cpp ^
    src/discovery_service.cpp ^
    src/folder_scanner.cpp ^
    src/sha256_hasher.cpp ^
    src/ui_utils.cpp ^
    -lws2_32

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo Build successful! Generated warp.exe
echo Run "warp.exe --help" for usage.
