@echo off
setlocal DisableDelayedExpansion
python "%~dp0example-build-wasi.py" %*
exit /b %errorlevel%
