@echo off
rem Builds better-pdf.exe with the development flags from STYLE.md.
setlocal

if not exist build mkdir build

clang -std=c23 -Wall -Wextra -pedantic -Werror -Og -g3 -D_CRT_SECURE_NO_WARNINGS ^
  -fsanitize=address,undefined -fno-sanitize-recover=all ^
  -Isrc -Ithird_party\pdfium\include ^
  src\*.c ^
  third_party\pdfium\lib\pdfium.dll.lib -lws2_32 -lshell32 -luser32 ^
  -o build\better-pdf.exe
if errorlevel 1 exit /b 1

copy /Y third_party\pdfium\bin\pdfium.dll build\ >nul
for /f "delims=" %%i in ('clang -print-resource-dir') do set RESOURCE_DIR=%%i
copy /Y "%RESOURCE_DIR%\lib\windows\clang_rt.asan_dynamic-x86_64.dll" build\ >nul
exit /b 0