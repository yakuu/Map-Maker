@echo off
setlocal EnableExtensions

:: ---- Map-Maker build & run launcher ----

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
set "CMAKE_GENERATOR_PLATFORM=x64"
set "UseMultiToolTask=true"
set "EnforceProcessCountAcrossBuilds=true"

:: ---- Force v143 toolset (critical) ----
set "VCPKG_PLATFORM_TOOLSET=v143"

:: ---- Point vcpkg at the overlay triplet folder (env var, not cache var!) ----
set "VCPKG_OVERLAY_TRIPLETS=%CD%\triplets"
set "VCPKG_TARGET_TRIPLET=x64-windows-static-v143"

:: Skip any prebuilt binary cache so we get a clean v143 build
set "VCPKG_BINARY_SOURCES=clear"

:: ---- Ensure the triplet file exists ----
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

echo [INFO] VCPKG_ROOT          = %VCPKG_ROOT%
echo [INFO] Triplet             = %VCPKG_TARGET_TRIPLET%
echo [INFO] Overlay triplets    = %VCPKG_OVERLAY_TRIPLETS%
echo [INFO] Platform toolset    = %VCPKG_PLATFORM_TOOLSET%
echo [INFO] Parallel            = %CMAKE_BUILD_PARALLEL_LEVEL% jobs

:: ---- Clean stale artifacts ----
if exist "build"           rmdir /s /q "build"
if exist "vcpkg_installed" rmdir /s /q "vcpkg_installed"
if exist "bin\Release"     rmdir /s /q "bin\Release"
if exist "bin\Debug"       rmdir /s /q "bin\Debug"

:: ---- Configure ----
cmake --preset default ^
  -DVCPKG_MANIFEST_MODE=ON ^
  -DVCPKG_OVERLAY_TRIPLETS="%VCPKG_OVERLAY_TRIPLETS%" ^
  -DVCPKG_TARGET_TRIPLET=%VCPKG_TARGET_TRIPLET% ^
  -DVCPKG_PLATFORM_TOOLSET=v143 ^
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ^
  -DCMAKE_EXE_LINKER_FLAGS="/OPT:REF /OPT:ICF /INCREMENTAL:NO" || goto :err

:: ---- Confirm vcpkg produced the right toolset ----
echo.
echo [CHECK] Looking for the freshly built assimp library...
for %%F in ("vcpkg_installed\x64-windows-static-v143\lib\assimp-*.lib") do (
    echo        %%F
)
echo        ^(must contain "vc143" -- if it says vc144 or vc145, stop and tell me^)
echo.

:: ---- Build ----
cmake --build --preset release --config Release --parallel %NUMBER_OF_PROCESSORS% || goto :err

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