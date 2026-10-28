@echo off
rem Rebuilds the program and runs the unit tests.
setlocal

call "%~dp0build.bat"
if errorlevel 1 exit /b 1
call "%~dp0test.bat"
exit /b %errorlevel%