@echo off
setlocal DisableDelayedExpansion

rem Request native SDK environment syntax even when called from Git Bash.
set "MSYSTEM="
set "EMSDK_BASH="
set "EMSDK_CMD=1"

if not defined EMSDK_VERSION set "EMSDK_VERSION=7.0.0"
if not defined SIXEL_EMCC_BUILD_DIR set "SIXEL_EMCC_BUILD_DIR=%~dp0build-emcc-windows"
for %%I in ("%~dp0..") do set "TOP_SRCDIR=%%~fI"
for %%I in ("%SIXEL_EMCC_BUILD_DIR%") do set "SIXEL_EMCC_BUILD_DIR=%%~fI"
set "PATH=%TOP_SRCDIR%\.local\bin;%PATH%"

if defined SIXEL_EMSDK_DIR goto setup_sdk
where emcc.exe >nul 2>&1 && goto sdk_ready
set "SIXEL_EMSDK_DIR=%~dp0emsdk-Windows-x86_64"

:setup_sdk
if not exist "%SIXEL_EMSDK_DIR%\emsdk.bat" (
  git clone --depth 1 --branch "%EMSDK_VERSION%" https://github.com/emscripten-core/emsdk.git "%SIXEL_EMSDK_DIR%" || exit /b 1
)
rem Let curl use the Windows certificate store when it is available.
where curl.exe >nul 2>&1 && set "EMSDK_USE_CURL=1"
call "%SIXEL_EMSDK_DIR%\emsdk.bat" install "%EMSDK_VERSION%" || exit /b 1
call "%SIXEL_EMSDK_DIR%\emsdk.bat" activate "%EMSDK_VERSION%" || exit /b 1
call "%SIXEL_EMSDK_DIR%\emsdk_env.bat" || exit /b 1

:sdk_ready
if defined EMSDK set "PATH=%EMSDK:/=\%\upstream\bin;%PATH%"
emcc.exe --version || exit /b 1

if not exist "%SIXEL_EMCC_BUILD_DIR%" mkdir "%SIXEL_EMCC_BUILD_DIR%" || exit /b 1
set "SIXEL_EMCC_RECONFIGURE="
if exist "%SIXEL_EMCC_BUILD_DIR%\meson-private\coredata.dat" set "SIXEL_EMCC_RECONFIGURE=--reconfigure"
meson setup %SIXEL_EMCC_RECONFIGURE% "%SIXEL_EMCC_BUILD_DIR%" "%TOP_SRCDIR%" ^
  --cross-file "%TOP_SRCDIR%\build-aux\meson-cross\emscripten-windows" ^
  --prefix / -Ddefault_library=static -Dauto_features=disabled -Dthreads=enabled -Dtests=false || exit /b 1
meson compile -C "%SIXEL_EMCC_BUILD_DIR%" || exit /b 1

set "WASM_STAGE=%SIXEL_EMCC_BUILD_DIR%\install"
meson install -C "%SIXEL_EMCC_BUILD_DIR%" --no-rebuild --destdir "%WASM_STAGE%" || exit /b 1
node "%WASM_STAGE%\bin\img2sixel.js" -o "%SIXEL_EMCC_BUILD_DIR%\example.six" "%TOP_SRCDIR%\images\snake.png" || exit /b 1
node "%WASM_STAGE%\bin\sixel2png.js" -i "%SIXEL_EMCC_BUILD_DIR%\example.six" -o "%SIXEL_EMCC_BUILD_DIR%\example.png" || exit /b 1
node "%WASM_STAGE%\bin\lsqa.js" -m ms-ssim -b ms-ssim:min=0.98 "%TOP_SRCDIR%\images\snake.png" "%SIXEL_EMCC_BUILD_DIR%\example.png"
exit /b %errorlevel%
