@echo off
rem Builds better-pdf.exe. Usage: build.bat [debug^|release]
rem
rem   debug   -Og -g3, assertions log and stop in the debugger with int3
rem   release -O2, no debug info, assertions compile away (default for use)
rem
rem The compiler is clang in C23 mode; the linker is the MSVC one.
setlocal

set MODE=%~1
if "%MODE%"=="" set MODE=debug
if /I "%MODE%"=="debug" goto flags_debug
if /I "%MODE%"=="release" goto flags_release
echo usage: scripts\build.bat [debug^|release]
exit /b 2

:flags_debug
rem /DEBUG writes better-pdf.pdb next to the exe so a debugger gets file, line,
rem and expression information for the trap.
set FLAGS=-Og -g3 -DBETTER_PDF_DEBUG -Wl,/DEBUG
goto compile

:flags_release
rem No /DEBUG and no -g: the release image carries no debug sections and no PDB.
rem REF and ICF let the linker drop unused code and fold identical functions.
set FLAGS=-O2 -DBETTER_PDF_RELEASE -Wl,/OPT:REF,/OPT:ICF

:compile
if not exist build mkdir build
if /I "%MODE%"=="release" if exist build\better-pdf.pdb del /Q build\better-pdf.pdb

clang -std=c23 -Wall -Wextra -pedantic -Werror -D_CRT_SECURE_NO_WARNINGS %FLAGS% ^
  -Isrc -Ithird_party\pdfium\include ^
  src\*.c ^
  third_party\pdfium\lib\pdfium.dll.lib -lws2_32 -lshell32 -luser32 ^
  -o build\better-pdf.exe
if errorlevel 1 exit /b 1

copy /Y third_party\pdfium\bin\pdfium.dll build\ >nul
echo built build\better-pdf.exe (%MODE%)
exit /b 0