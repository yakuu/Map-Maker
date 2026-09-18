@echo off
setlocal EnableExtensions

:: ---- Map-Maker build & run launcher (FAST / incremental) ----
:: Run from the Map-Maker folder. Dependencies are NOT reinstalled or rebuilt.

cd /d "%~dp0" || goto :err

if not exist "vcpkg.json" (
    echo [ERROR] vcpkg.json not found in "%CD%". Run from the Map-Maker folder.
    pause
    exit /b 1
)

:: ---- Environment ----
set "VCPKG_ROOT=C:\vcpkg"
set "CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
set "CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%"
set "UseMultiToolTask=true"
set "EnforceProcessCountAcrossBuilds=true"
set "VCPKG_PLATFORM_TOOLSET=v143"
set "VCPKG_OVERLAY_TRIPLETS=%CD%\triplets"
set "VCPKG_TARGET_TRIPLET=x64-windows-static-v143"

:: FIX: Correct syntax for binary sources. Use 'files' to enable local caching.
set "VCPKG_BINARY_SOURCES=files,C:\Users\Administrator\AppData\Local\vcpkg\archives"

echo [INFO] Triplet   = %VCPKG_TARGET_TRIPLET%
echo [INFO] Parallel  = %CMAKE_BUILD_PARALLEL_LEVEL% jobs

:: ---- Ensure the triplet file exists (only needed once) ----
if not exist "triplets" mkdir "triplets"
if not exist "triplets\x64-windows-static-v143.cmake" (
    echo [INFO] Writing triplets\x64-windows-static-v143.cmake
    > "triplets\x64-windows-static-v143.cmake" (
        echo set^(VCPKG_TARGET_ARCHITECTURE x64^)
        echo set^(VCPKG_CRT_LINKAGE static^)
        echo set^(VCPKG_LIBRARY_LINKAGE static^)
        echo set^(VCPKG_PLATFORM_TOOLSET v143^)
    )
)

:: ---- First-run only: full configure + dependency build ----
if not exist "build\CMakeCache.txt" (
    echo [INFO] No cached configure found - running FULL setup ^(this is the slow one^).
    cmake --preset default ^
      -DVCPKG_MANIFEST_MODE=ON ^
      -DVCPKG_OVERLAY_TRIPLETS="%VCPKG_OVERLAY_TRIPLETS%" ^
      -DVCPKG_TARGET_TRIPLET=%VCPKG_TARGET_TRIPLET% ^
      -DVCPKG_PLATFORM_TOOLSET=v143 ^
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ^
      -DCMAKE_EXE_LINKER_FLAGS="/OPT:REF /OPT:ICF /INCREMENTAL:NO" || goto :err
) else (
    echo [INFO] Cache detected - skipping vcpkg install, reusing vcpkg_installed\.
)

:: ---- Build ONLY the project target (no port rebuilds, no clean) ----
cmake --build --preset release --config Release --target MapMaker --parallel %NUMBER_OF_PROCESSORS% || goto :err

:: ---- Locate and launch ----
set "EXE="
for %%P in ("bin\MapMaker.exe" "bin\Release\MapMaker.exe") do (
    if exist %%P set "EXE=%%~P"
)
if not defined EXE (
    echo [ERROR] MapMaker.exe not found after build.
    pause
    exit /b 1
)

echo [SUCCESS] Launching "%EXE%"
start "" "%EXE%"
endlocal
exit /b 0

:err
echo.
echo [FAILED] Aborted at errorlevel %errorlevel%
pause
exit /b 1