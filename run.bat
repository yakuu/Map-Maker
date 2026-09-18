@echo off
setlocal EnableExtensions

:: ---- Map-Maker build & run launcher ----
:: Run this from the Map-Maker folder (where vcpkg.json lives).

cd /d "%~dp0" || goto :err

if not exist "vcpkg.json" (
    echo [ERROR] vcpkg.json not found in "%CD%". Run from the Map-Maker folder.
    pause
    exit /b 1
)

:: ---- Environment / optimization switches ----
set "VCPKG_ROOT=C:\vcpkg"
set "CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
set "VCPKG_TARGET_TRIPLET=x64-windows-static"
set "CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%"
set "CMAKE_GENERATOR_PLATFORM=x64"

:: MSVC: keep cl.exe/link.exe warm across compiles for faster rebuilds
set "UseMultiToolTask=true"
set "EnforceProcessCountAcrossBuilds=true"

echo [INFO] VCPKG_ROOT   = %VCPKG_ROOT%
echo [INFO] Triplet      = %VCPKG_TARGET_TRIPLET%
echo [INFO] Parallel     = %CMAKE_BUILD_PARALLEL_LEVEL% jobs

:: ---- Generate / configure (Release, LTO/IPO on) ----
cmake --preset default ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DVCPKG_MANIFEST_MODE=ON ^
  -DVCPKG_MANIFEST_FEATURES="" ^
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ^
  -DCMAKE_EXE_LINKER_FLAGS="/OPT:REF /OPT:ICF /INCREMENTAL:NO" || goto :err

:: ---- Build (Release, all cores, no incremental link) ----
cmake --build --preset release --config Release --parallel %NUMBER_OF_PROCESSORS% || goto :err

:: ---- Locate and launch the binary ----
set "EXE="
for %%P in ("bin\MapMaker.exe" "bin\Release\MapMaker.exe" "build\bin\Release\MapMaker.exe" "build\Release\MapMaker.exe") do (
    if exist %%P set "EXE=%%~P"
)

if not defined EXE (
    echo [ERROR] MapMaker.exe not found after build. Check your CMake output paths.
    pause
    exit /b 1
)

echo [SUCCESS] Launching "%EXE%"
start "" "%EXE%"
endlocal
exit /b 0

:err
echo.
echo [FAILED] Aborted at errorlevel !errorlevel!
pause
exit /b 1