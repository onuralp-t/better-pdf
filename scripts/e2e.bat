@echo off
rem Runs the real pipeline end to end on small page ranges of the sample books.
rem This needs the vendored model and several minutes of CPU time.
setlocal

call "%~dp0build.bat"
if errorlevel 1 exit /b 1

if not exist build\e2e mkdir build\e2e

echo.
echo === clean.pdf pages 2-6 ===
build\better-pdf.exe samples\clean.pdf build\e2e\clean.pdf --pages 2-6
if errorlevel 1 exit /b 1

echo.
echo === bloated.pdf pages 155-158 ===
build\better-pdf.exe samples\bloated.pdf build\e2e\bloated.pdf --pages 155-158
if errorlevel 1 exit /b 1

echo.
echo === two-column-bloated.pdf pages 80-82 ===
build\better-pdf.exe samples\two-column-bloated.pdf build\e2e\two-column.pdf --pages 80-82
if errorlevel 1 exit /b 1

rem The whole book takes hours on CPU; run it by hand when you want a full result:
rem   build\better-pdf.exe samples\clean.pdf build\e2e\clean-full.pdf
exit /b 0