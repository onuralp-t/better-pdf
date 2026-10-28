@echo off
rem Builds and runs the unit tests under AddressSanitizer and UBSan.
setlocal

call "%~dp0build_tests.bat"
if errorlevel 1 exit /b 1

build\unit_tests.exe
exit /b %errorlevel%