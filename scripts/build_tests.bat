@echo off
rem Builds the unit test binary with the development flags from STYLE.md.
setlocal

if not exist build mkdir build

clang -std=c23 -Wall -Wextra -pedantic -Werror -Og -g3 -D_CRT_SECURE_NO_WARNINGS ^
  -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc -Itests ^
  src\log.c src\arena.c src\asserts.c src\cli.c src\platform_win32.c ^
  src\base64.c src\png.c src\image.c src\json.c src\http_message.c src\gguf.c src\ocr_text.c src\document.c src\geometry.c src\typst_emit.c ^
  tests\test_main.c tests\test_log.c tests\test_arena.c tests\test_cli.c ^
  tests\test_base64.c tests\test_png.c tests\test_image.c ^
  tests\test_json.c tests\test_http_message.c tests\test_gguf.c ^
  tests\test_ocr_text.c tests\test_document.c tests\test_geometry.c tests\test_typst_emit.c ^
  -lws2_32 -lshell32 -luser32 -o build\unit_tests.exe
if errorlevel 1 exit /b 1

rem AddressSanitizer is a dynamic runtime on Windows: the DLL must sit next to
rem the executable or the loader fails with 0xC0000139 before main() runs.
for /f "delims=" %%i in ('clang -print-resource-dir') do set RESOURCE_DIR=%%i
copy /Y "%RESOURCE_DIR%\lib\windows\clang_rt.asan_dynamic-x86_64.dll" build\ >nul
exit /b 0