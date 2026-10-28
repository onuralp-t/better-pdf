@echo off
rem Runs the unit tests with coverage instrumentation and prints branch and
rem MC/DC summaries for the code the tests are meant to cover.
setlocal

call "%~dp0build_coverage.bat"
if errorlevel 1 exit /b 1

set LLVM_PROFILE_FILE=build\coverage.profraw
build\unit_tests_cov.exe
if errorlevel 1 exit /b 1

llvm-profdata merge -sparse build\coverage.profraw -o build\coverage.profdata
if errorlevel 1 exit /b 1

rem The Win32 adapter, the pdfium binding, the HTTP client, the child processes,
rem and the orchestrator contain no decision logic of their own; they are
rem exercised by scripts\e2e.bat instead. The assertion helper is only reachable
rem by trapping, which the coverage runtime cannot record.
llvm-cov report build\unit_tests_cov.exe -instr-profile build\coverage.profdata ^
  -ignore-filename-regex="platform_win32\.c|pdf_input\.c|ocr_client\.c|server_process\.c|typst_run\.c|pipeline_run\.c|main\.c|asserts\.c|tests" ^
  -show-branch-summary -show-mcdc-summary
exit /b %errorlevel%