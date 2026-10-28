@echo off
rem Builds build\unit_tests.exe. Usage: build_tests.bat [debug^|release]
setlocal

set MODE=%~1
if "%MODE%"=="" set MODE=debug
if /I "%MODE%"=="debug" goto flags_debug
if /I "%MODE%"=="release" goto flags_release
echo usage: scripts\build_tests.bat [debug^|release]
exit /b 2

:flags_debug
rem AddressSanitizer and UndefinedBehaviorSanitizer stay on while developing,
rem as STYLE.md asks.
set FLAGS=-Og -g3 -DBETTER_PDF_DEBUG -fsanitize=address,undefined
goto compile

:flags_release
set FLAGS=-O2 -DBETTER_PDF_RELEASE

:compile
if not exist build mkdir build

clang -std=c23 -Wall -Wextra -pedantic -Werror -D_CRT_SECURE_NO_WARNINGS %FLAGS% ^
  -Isrc -Itests ^
  src\log.c src\arena.c src\asserts.c src\cli.c src\platform_win32.c ^
  src\base64.c src\png.c src\image.c src\json.c src\http_message.c src\gguf.c src\ocr_text.c src\document.c src\geometry.c src\typst_emit.c src\cache.c src\pipeline.c ^
  tests\test_main.c tests\test_log.c tests\test_arena.c tests\test_cli.c ^
  tests\test_base64.c tests\test_png.c tests\test_image.c ^
  tests\test_json.c tests\test_http_message.c tests\test_gguf.c ^
  tests\test_ocr_text.c tests\test_document.c tests\test_geometry.c tests\test_typst_emit.c tests\test_cache.c tests\test_pipeline.c ^
  -lws2_32 -lshell32 -luser32 -o build\unit_tests.exe
if errorlevel 1 exit /b 1

echo built build\unit_tests.exe (%MODE%)
exit /b 0