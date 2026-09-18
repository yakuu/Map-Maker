# Map-Maker

Windows-only C++20 OpenGL editor.

## Prereqs
- Visual Studio 2022 (Desktop C++)
- CMake 3.21+
- vcpkg with `VCPKG_ROOT` set

## Build
    scripts\create_project.bat
    cmake --preset default
    cmake --build --preset release

Run: `bin\MapMaker.exe`

## Controls
- WASD + QE: free-fly
- Right mouse: capture cursor for camera look
- Tab (hold): radial tool menu
- G: toggle GAT overlay
- Z / Y: undo / redo
- F5: save