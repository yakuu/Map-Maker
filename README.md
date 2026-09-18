# Map-Maker

A Windows-only, C++20, immediate-mode level editor built on OpenGL 4.6, GLFW, Dear ImGui (docking), and assimp. It targets large, highly instanced scenes with a hotkey-first UI, live-reloading config, and a fixed on-disk layout.

## Prereqs

| Prerequisite | Requirement | Notes |
| --- | --- | --- |
| Windows | 10/11 x64 | Only supported platform. |
| Visual Studio 2022 | 17.10+ | Must have the **Desktop development with C++** workload. Requires the v143 toolset (14.4x) to match vcpkg-built dependencies. |
| CMake | 3.21+ | Ships with recent VS installs; `cmake --version` to verify. |
| vcpkg | latest | Install once, clone to e.g. `C:\vcpkg`, run `bootstrap-vcpkg.bat` once. |
| Git | any | Only if you plan to version the project. |

The project expects the environment variable `VCPKG_ROOT` to point at your vcpkg clone. `run.bat` sets it locally to `C:\vcpkg` if it isn't set globally.

### Toolset gotcha (read this first)

vcpkg-built dependencies must use the same MSVC toolset as your project.

If your VS install ships a newer toolset (v144 / v145), vcpkg will pick it up and the linker will fail with unresolved `__std_*` symbols.

The repo pins v143 via:

- `triplets/x64-windows-static-v143.cmake` — forces vcpkg to build with v143.
- `CMakePresets.json` — sets `"toolset": "v143"` and the custom triplet.
- `run.bat` — exports `VCPKG_OVERLAY_TRIPLETS` as an environment variable (this is the one that actually matters; the CMake cache variable is ignored during manifest resolution).

## Build

```bat
cmake --preset default
cmake --build --preset release --parallel
bin\MapMaker.exe
```

Run: `run.bat` for automatic build - starts the MapMaker.exe

## Directory layout

```text
MapMaker/
├── CMakeLists.txt           Project definition.
├── CMakePresets.json        Configure/build presets.
├── vcpkg.json               Manifest: glfw3, glm, nlohmann-json, imgui, assimp.
├── run.bat                  Build-and-launch script (recommended entry point).
│
├── triplets/                Custom vcpkg triplets (v143).
├── scripts/                 Utility scripts (tree creation, etc.).
│
├── config/                  Live-reloaded JSON configuration.
│   ├── shortcuts.json
│   ├── tools.json
│   ├── render.json
│   └── materials.json
│
├── Assets/                  Source meshes (.obj .fbx .gltf .glb .dae .ply .stl .3ds).
├── Texture/                 Albedo / normal / roughness atlases (unused by current build).
├── Material/                .mat descriptors (unused by current build).
├── Save/                    Serialized maps (map.json) and exports.
│
├── src/                     C++ sources.
└── bin/                     Build output + runtime working directory.
```

## Source layout

```text
src/
├── main.cpp                     Entry point.
│
├── Core/
│   ├── Application.{h,cpp}      Owns the window, frame loop, UI, and shared state.
│   ├── Config.{h,cpp}           Multi-file JSON loader with mtime-based hot reload.
│   ├── Shortcuts.{h,cpp}        Keyboard + mouse binding table with load/save.
│   ├── Commands.h               Command pattern: CommandStack + common commands.
│   └── Toast.h                  Transient non-blocking notifications.
│
├── Render/
│   ├── GL.{h,cpp}               Minimal hand-rolled OpenGL 4.6 loader.
│   ├── Shader.{h,cpp}           Program compile/link + uniform helpers.
│   ├── Mesh.{h,cpp}             Vertex/index buffers, VAO wrapper, factory shapes.
│   ├── Framebuffer.{h,cpp}      Render target for the viewport image.
│   ├── InstanceRenderer.{h,cpp} Batched instanced draws via an SSBO.
│   ├── AssetImporter.{h,cpp}    assimp wrapper: file → Vertex/Index arrays.
│   └── AssetRegistry.{h,cpp}    Recursive asset scan, dedupe, hash table.
│
├── Scene/
│   ├── Camera.{h,cpp}           Free-fly camera.
│   └── Scene.{h,cpp}            Instances, GAT grid, texture layers, heightmap.
│
├── Tools/
│   ├── ITool.h                  Tool interface (input hooks, ImGui panel, quick menu).
│   └── ToolManager.{h,cpp}      Tool registry, quick menus, radial menu, status bar.
│
└── Save/
    └── SaveIO.{h,cpp}           Read/write Save/map.json (v2 schema).
```

## Architecture

### Frame loop (`Application::frame`)

1. `glfwPollEvents`, hot-reload config, poll shortcuts, update toasts.
2. Handle camera capture (right mouse or middle mouse over viewport).
3. Update camera (suppressed while a Blender-style transform is running).
4. Update tools.
5. Rebuild the heightmap mesh if its version counter changed.
6. Begin ImGui frame → draw UI (dock host, viewport, panels, toasts, status bar).
7. Render the scene into `viewportFbo`.
8. Blit the ImGui draw lists onto the default framebuffer.
9. Swap.

The scene render is entirely instanced: every mesh in the renderer's registry is submitted once per unique asset, with per-instance transforms packed into a single SSBO (`layout(std430, binding=0)`), so draw calls are O(unique meshes), not O(instances).

## Tools

All tools implement `ITool` (see `src/Tools/ITool.h`). The three concrete categories of callback:

- **Input** — `onMouseDown` / `onMouseMove` / `onMouseUp` and `onUpdate` (poll-based for keyboard).
- **UI** — `onImGui` for the full panel; `drawQuickMenu` for the right-click popup.
- **Lifecycle** — `onActivate` / `onDeactivate`.

`ToolManager::update` calls `onUpdate` on every tool every frame, not just the active one. This is deliberate: the Transform tool needs to receive G/R/S regardless of which tool is currently selected.

## Save format (`Save/map.json`)

```json
{
  "version": 2,
  "instances": [
    {
      "id": 1,
      "meshHash": 140733193388031,
      "meshName": "cube",
      "position": [0.0, 1.2, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "scale":    [1.0, 1.0, 1.0],
      "tint":     [0.6, 0.7, 0.5, 1.0],
      "materialIndex": 0
    }
  ],
  "gat": {
    "w": 64, "h": 64, "cellSize": 1.0,
    "origin": [-32.0, 0.0, -32.0],
    "cells": [0, 0, 1, 1, ...],
    "textureLayers": [0, 0, 2, 2, ...]
  },
  "heightmap": {
    "w": 33, "h": 33, "cell": 1.5,
    "origin": [-24.0, 0.0, -24.0],
    "heights": [0.0, 0.0, ...]
  }
}
```

`meshHash` is a 64-bit hash of the asset's absolute path at load time.

Loading a save on a different machine will fail to find meshes whose paths have changed — the fallback is a missing-mesh skip, not a crash.

If you need portability, change `AssetRegistry::hashPath` to hash the path relative to `Assets/` and store that instead.

## Troubleshooting

### `error LNK2001: unresolved external symbol "__std_rotate"` (or `__std_unique_8`, `__std_find_first_not_of_trivial_pos_1`)

Toolset mismatch between your build and vcpkg-built dependencies. Confirm:

```bat
dir vcpkg_installed\x64-windows-static-v143\lib\assimp-vc143-mt.lib
```

If it says vc144 or vc145, your `triplets/` folder isn't being picked up. Make sure `run.bat` prints a non-empty Overlay triplets path and that `triplets\x64-windows-static-v143.cmake` exists. Then rebuild.

`_MSVC_STL_HARDENING=0` is not a fix for this; it only affects your own translation units.

### `error C2039: "DockBuilder*" is not a member of "ImGui"`

`#include <imgui_internal.h>` is missing. It must be included after `<imgui.h>`.

### `error C2065: "GLFW_KEY_LEFT_CTRL"`

GLFW names it `GLFW_KEY_LEFT_CONTROL` (and `RIGHT_CONTROL`, `LEFT_SHIFT`, `RIGHT_SHIFT`, `LEFT_ALT`, `RIGHT_ALT`). `Shortcuts::keyName` already handles the correct names.

### `error C2065: "ImGuiDockNodeFlags_DockSpace"`

Same as above — include `<imgui_internal.h>`.

### Invalid key `1` toast at startup

A shortcut is bound to `1`, which GLFW rejects as a keyboard key. Use the mouse codes (1000+) or a valid `GLFW_KEY_*` value.

### Black viewport

The viewport FBO may have failed to allocate. Check stderr for `[fbo] incomplete: 0x....`. This usually means the GL context wasn't created at 4.6 core — check that your GPU driver supports it and that no other GL program is holding the context.

### "Missing GL function: glXxx"

The requested function isn't in `LoadGLFunctions`. Add it to `GL.h` (typedef + extern), `GL.cpp` (definition + `LOAD(...)`), and rebuild. See convention #6 above.

### Panels look empty

Delete `bin/imgui.ini` or use **View → Reset layout**. The default layout is built once per process the first time `imgui.ini` doesn't exist.

## Controls

| Key | Action | Binding |
| --- | --- | --- |
| W / A / S / D | Move camera forward/left/back/right | `camera.forward` / `camera.left` / `camera.back` / `camera.right` |
| E / Q | Move camera up / down | `camera.up` / `camera.down` |
| Left Shift | Fast move (4×) | `camera.fast` |
| Right mouse (hold) | Capture cursor for look | `camera.capture` |
| Middle mouse (hold) | Capture cursor for look | `camera.lookMB` |
| Tab (hold) | Radial tool menu | `tool.radial` |
| Esc | Release cursor / cancel | `tool.cancel` |
| Z | Undo | `edit.undo` |
| Y | Redo | `edit.redo` |
| Delete | Delete selected instance | `edit.delete` |
| F5 | Save to `Save/map.json` | `save.quick` |
| F4 | Toggle GAT overlay | `view.toggleGat` |
| L | Toggle texture-layer overlay | `view.toggleLandscape` |

### Transform (Blender-inspired)

| Key | Action |
| --- | --- |
| G | Grab (move) |
| R | Rotate |
| S | Scale |
| X / Y / Z | Constrain to axis (press again to release) |
| Shift (held) | Precise mode — 1/4 speed |
| Enter or LMB | Confirm and push a single undo command |
| Esc or RMB | Cancel and restore original transform |

## Known gaps / next steps

The project is functional but the following items from the original spec are stubbed or not implemented:

- Texture atlas packing. `Texture/` exists but no packer is wired up. Materials in `materials.json` are display-only.
- Material graph editor. Not started.
- Landscape erosion and level modes. Only Raise, Lower, Smooth, Flat are implemented.
- Undo for landscape edits. Currently landscape strokes are not wrapped in commands.
- Undo for texture paint. Same — no command wrapper yet.
- Texture-array-backed atlas rendering. The instanced shader ignores `textureLayers`; only the overlay decal uses it.
- Save compression. The spec called for compressed JSON + binary sidecars; the current build writes plain JSON.
- Shortcut remapping UI. The map is editable via `shortcuts.json` and reloads live, but there's no in-app rebinding widget.
- Event/walkable trigger system. `GatCell::EventWalkable` exists but nothing evaluates the trigger flag yet.

The remaining spec items are marked with `TODO` comments in the code — `grep -rn TODO src/` for the full list.