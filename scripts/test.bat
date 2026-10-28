@echo off
rem Builds and runs the unit tests. Usage: test.bat [debug^|release]
setlocal

call "%~dp0build_tests.bat" %1
if errorlevel 1 exit /b 1

build\unit_tests.exe
exit /b %errorlevel%