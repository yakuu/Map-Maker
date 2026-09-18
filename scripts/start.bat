@echo off
setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
pushd "%ROOT%"

echo Creating Map-Maker project tree at: %ROOT%

for %%D in (
  "config" "scripts" "bin" "triplets"
  "src" "src\Core" "src\Render" "src\Scene" "src\Tools" "src\Save"
  "Assets" "Texture" "Material" "Save"
) do (
  if not exist "%%~D" mkdir "%%~D"
)

for %%F in (
  "vcpkg.json"
  "CMakeLists.txt"
  "CMakePresets.json"
  "README.md"
  "config\shortcuts.json"
  "config\tools.json"
  "config\render.json"
  "config\materials.json"
  "src\main.cpp"
  "src\Core\Application.h"
  "src\Core\Application.cpp"
  "src\Core\Config.h"
  "src\Core\Config.cpp"
  "src\Core\Shortcuts.h"
  "src\Core\Shortcuts.cpp"
  "src\Core\Commands.h"
  "src\Core\Toast.h"
  "src\Render\GL.h"
  "src\Render\GL.cpp"
  "src\Render\Shader.h"
  "src\Render\Shader.cpp"
  "src\Render\Mesh.h"
  "src\Render\Mesh.cpp"
  "src\Render\Framebuffer.h"
  "src\Render\Framebuffer.cpp"
  "src\Render\InstanceRenderer.h"
  "src\Render\InstanceRenderer.cpp"
  "src\Render\AssetImporter.h"
  "src\Render\AssetImporter.cpp"
  "src\Render\AssetRegistry.h"
  "src\Render\AssetRegistry.cpp"
  "src\Scene\Camera.h"
  "src\Scene\Camera.cpp"
  "src\Scene\Scene.h"
  "src\Scene\Scene.cpp"
  "src\Tools\ITool.h"
  "src\Tools\ToolManager.h"
  "src\Tools\ToolManager.cpp"
  "src\Save\SaveIO.h"
  "src\Save\SaveIO.cpp"
) do (
  if not exist "%%~F" type nul > "%%~F"
)

echo.
echo Tree created. Paste each file's contents into place.
popd
endlocal