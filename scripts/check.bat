@echo off
rem Rebuilds the program and runs the unit tests.
rem Usage: check.bat [debug^|release]
setlocal

call "%~dp0build.bat" %1
if errorlevel 1 exit /b 1
call "%~dp0test.bat" %1
exit /b %errorlevel%