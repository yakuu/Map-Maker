# Map Format

This document describes the JSON map format that Map-Maker writes, and shows
how to consume it in another program — a game, a renderer, a viewer, whatever
you like. Nothing here depends on Map-Maker's source; the format is
self-contained and stable.

If you just want to look at a real file, open `Save/map.json` after placing a
few objects in the editor.

## Contents

- [Overview](#overview)
- [The three sections](#the-three-sections)
- [Coordinate conventions](#coordinate-conventions)
- [Loading meshes – the meshPath design](#loading-meshes---the-meshpath-design)
- [Minimal loader](#minimal-loader)
- [Minimal renderer](#minimal-renderer)
- [Common pitfalls](#common-pitfalls)
- [Beyond the minimum](#beyond-the-minimum)
- [Reference](#reference)

## Overview

A map file is a single JSON object with three top-level keys, plus a version
stamp:

```json
{
  "version": 2,
  "instances": [ ... ],
  "gat": { ... },
  "heightmap": { ... }
}
```

| Key | Contents | Consumed by |
| --- | --- | --- |
| `instances` | Placed objects: a mesh reference plus a transform | Your scene graph |
| `gat` | Grid of walkability flags and texture-layer indices | Collision / nav / game logic |
| `heightmap` | Grid of terrain heights | Terrain mesh, or sampled for ground Y |

`version` is currently 2. If a future format change is incompatible, this
number will change. Always check it.

## The three sections

### instances

One entry per placed object.

```json
{
  "id": 51,
  "materialIndex": 0,
  "meshHash": 12683337214813280131,
  "meshName": "corridor-corner.obj",
  "meshPath": "Assets/map/obj/corridor-corner.obj",
  "position": [ 5.016, 0.0, -0.963 ],
  "rotation": [ 0.0, 0.0, 0.0 ],
  "scale": [ 1.0, 1.0, 1.0 ],
  "tint": [ 1.0, 1.0, 1.0, 1.0 ]
}
```

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | int | Unique within the file. Not stable across saves – treat as an identifier, not a reference. |
| `materialIndex` | int | Reserved. Currently always 0. |
| `meshHash` | uint64 | Stable hash of `meshPath`. Use this as a cache key – two instances with the same hash refer to the same mesh file. |
| `meshName` | string | Human-readable basename (no directory). |
| `meshPath` | string | Forward-slash path to the mesh file. This is the field you load. See the meshPath design below. |
| `position` | [x, y, z] | World-space position, meters. See conventions. |
| `rotation` | [rx, ry, rz] | Degrees, XYZ Euler. See conventions. |
| `scale` | [sx, sy, sz] | Unitless. Usually [1,1,1]. |
| `tint` | [r, g, b, a] | Multiplied into the material color. Range 0–1. |

### gat

Grid of walkability flags and texture-layer indices. Same dimensions for both.

```json
{
  "w": 64,
  "h": 64,
  "cellSize": 1.0,
  "origin": [ -32.0, 0.0, -32.0 ],
  "cells": [ 0, 0, 0, ... ],
  "textureLayers": [ 0, 0, 0, ... ]
}
```

`cells` is row-major, `w * h` entries. Values:

- `0` – Walkable
- `1` – Not walkable
- `2` – Event-walkable (walkable, but triggers something)

`textureLayers` is the same shape. Value is a layer index 0–7. `0` means "no layer
assigned".

`origin` is the world-space position of the corner of cell (0, 0).

`cellSize` is the width of one cell, in meters.

To convert a world position (x, z) to a cell index:

```cpp
int cx = (int)std::floor((x - origin.x) / cellSize);
int cy = (int)std::floor((z - origin.z) / cellSize);
```

Bounds-check both against `w` and `h` before indexing.

### heightmap

Grid of terrain heights. Same layout as `gat`, but the values are float heights.

```json
{
  "w": 65,
  "h": 65,
  "cell": 1.5,
  "origin": [ -48.0, 0.0, -48.0 ],
  "heights": [ 0.0, 0.0, 0.0, ... ]
}
```

`heights` is row-major, `w * h` entries, in meters.

The heightmap is a **vertex grid**, not a cell grid. There are `w` sample points
along X and `h` along Z, and `(w-1) * (h-1) * 2` triangles if you triangulate it.

`cell` is the distance between adjacent samples. Note it's called `cell` here but
`cellSize` in the `gat` section – historical, sorry.

Sample `(i, j)` sits at world `(origin.x + i * cell, origin.y + heights[j*w + i],
origin.z + j * cell)`.

To sample the height at an arbitrary world position, bilinearly interpolate
between the four nearest samples.

## Coordinate conventions

Y is up. X is east-west, Z is north-south. The same as most modern 3D engines.

Units are meters. A prop that's 2 units tall in your DCC tool is 2 meters tall
in the map.

Rotations are XYZ Euler in degrees. Meaning: apply X rotation first, then Y,
then Z. The matrix is built as:

```
M = T * Rx(rot.x) * Ry(rot.y) * Rz(rot.z) * S
```

In GLM:

```cpp
glm::mat4 m(1.0f);
m = glm::translate(m, position);
m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1, 0, 0));
m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0, 1, 0));
m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0, 0, 1));
m = glm::scale(m, scale);
```

Getting the order wrong produces subtly wrong angles on anything rotated on
more than one axis.

The two grids can have different dimensions. In a typical map, the GAT is 64x64
at 1 m cells (~64 m across) and the heightmap is 65x65 at 1.5 m cells (~96 m
across). Don't assume they match.

## Loading meshes – the meshPath design

The `meshPath` field is intentionally the only place a mesh is referenced, and
the design has three properties worth understanding.

1. **It's the export-time path.** Map-Maker scans an `Assets/` folder, hashes each
   file's path, and stores that hash. When you save, it writes both the hash
   and a normalized path. The path you see in the JSON is what Map-Maker
   resolved at save time – it's already been through Map-Maker's own root
   stripping and re-prefixing.

2. **It's forward-slash normalized.** Windows separators are rewritten to `/` at
   save time. Loaders on every platform can use it directly.

3. **It's relative to nothing in particular – you decide the root.** The typical
   pattern is to prefix it with a known asset root at load time:

   ```cpp
   std::string full = assetRoot + "/" + inst.meshPath;
   ```

   If `meshPath` is `"Assets/map/obj/corridor.obj"` and your game keeps its meshes
   at `<game>/assets/`, you'd pass `assetRoot = "assets"` and load
   `assets/Assets/map/obj/corridor.obj`. Ugly. Better: set the prefix Map-Maker
   writes to match your game's layout – see the note in the Map-Maker README
   about `kAssetPrefix` in `SaveIO.cpp`. Once you've matched them, `assetRoot` is
   just your game's working directory, usually `""`.

If you find a `meshPath` that doesn't resolve, treat it as a soft failure: log
it, skip the instance, keep going. A map with one broken mesh is still useful;
a map that refuses to load because of one missing file is not.

## Minimal loader

C++17, nlohmann/json. Install with `vcpkg install nlohmann-json` or drop the
single header in.

**map_loader.hpp:**

```cpp
#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct MapInstance {
    int id = 0;
    uint64_t meshHash = 0;
    std::string meshName;
    std::string meshPath;
    glm::vec3 position{ 0.0f };
    glm::vec3 rotation{ 0.0f }; // degrees, XYZ
    glm::vec3 scale{ 1.0f };
    glm::vec4 tint{ 1.0f };
};

struct MapGat {
    int w = 0, h = 0;
    float cellSize = 1.0f;
    glm::vec3 origin{ 0.0f };
    std::vector<uint8_t> cells;
    std::vector<uint8_t> textureLayers;
};

struct MapHeightmap {
    int w = 0, h = 0;
    float cell = 1.0f;
    glm::vec3 origin{ 0.0f };
    std::vector<float> heights;
};

struct MapData {
    std::vector<MapInstance> instances;
    MapGat gat;
    MapHeightmap heightmap;
};

bool loadMap(const std::string& jsonPath, MapData& out, std::string& errorOut);
```

**map_loader.cpp:**

```cpp
#include "map_loader.hpp"

#include <nlohmann/json.hpp>
#include <fstream>

using nlohmann::json;

namespace {

glm::vec3 readVec3(const json& j, const char* key, glm::vec3 fallback) {
    if (!j.contains(key)) return fallback;
    const auto& a = j[key];
    if (!a.is_array() || a.size() != 3) return fallback;
    return { a[0].get<float>(), a[1].get<float>(), a[2].get<float>() };
}

glm::vec4 readVec4(const json& j, const char* key, glm::vec4 fallback) {
    if (!j.contains(key)) return fallback;
    const auto& a = j[key];
    if (!a.is_array() || a.size() != 4) return fallback;
    return { a[0].get<float>(), a[1].get<float>(),
             a[2].get<float>(), a[3].get<float>() };
}

} // namespace

bool loadMap(const std::string& jsonPath, MapData& out, std::string& errorOut) {
    std::ifstream in(jsonPath, std::ios::binary);
    if (!in) { errorOut = "Cannot open " + jsonPath; return false; }

    json j;
    try { in >> j; }
    catch (const std::exception& e) { errorOut = e.what(); return false; }

    if (j.value("version", 0) != 2) {
        errorOut = "Unsupported map version";
        return false;
    }

    out = MapData{};

    // ----- Instances -----
    if (j.contains("instances")) {
        for (const auto& ji : j["instances"]) {
            MapInstance inst;
            inst.id = ji.value("id", 0);
            inst.meshHash = ji.value("meshHash", (uint64_t)0);
            inst.meshName = ji.value("meshName", std::string{});
            inst.meshPath = ji.value("meshPath", std::string{});
            inst.position = readVec3(ji, "position", {0, 0, 0});
            inst.rotation = readVec3(ji, "rotation", {0, 0, 0});
            inst.scale = readVec3(ji, "scale", {1, 1, 1});
            inst.tint = readVec4(ji, "tint", {1, 1, 1, 1});
            out.instances.push_back(std::move(inst));
        }
    }

    // ----- GAT -----
    if (j.contains("gat")) {
        const auto& g = j["gat"];
        out.gat.w = g.value("w", 0);
        out.gat.h = g.value("h", 0);
        out.gat.cellSize = g.value("cellSize", 1.0f);
        out.gat.origin = readVec3(g, "origin", {0, 0, 0});

        if (g.contains("cells") && out.gat.w > 0 && out.gat.h > 0) {
            out.gat.cells.reserve((size_t)out.gat.w * out.gat.h);
            for (const auto& c : g["cells"]) {
                out.gat.cells.push_back((uint8_t)c.get<int>());
            }
        }
        if (g.contains("textureLayers")) {
            out.gat.textureLayers.reserve((size_t)out.gat.w * out.gat.h);
            for (const auto& c : g["textureLayers"]) {
                out.gat.textureLayers.push_back((uint8_t)c.get<int>());
            }
        }
    }

    // ----- Heightmap -----
    if (j.contains("heightmap")) {
        const auto& h = j["heightmap"];
        out.heightmap.w = h.value("w", 0);
        out.heightmap.h = h.value("h", 0);
        out.heightmap.cell = h.value("cell", 1.0f);
        out.heightmap.origin = readVec3(h, "origin", {0, 0, 0});

        if (h.contains("heights")) {
            out.heightmap.heights.reserve((size_t)out.heightmap.w * out.heightmap.h);
            for (const auto& v : h["heights"]) {
                out.heightmap.heights.push_back(v.get<float>());
            }
        }
    }

    return true;
}
```

That's it. Everything else is what you do with the data.

## Minimal renderer

OpenGL fixed-function, assimp for mesh loading. If you're on a modern GL
pipeline, swap the fixed-function calls for a shader – the loader doesn't care.

**static_mesh.hpp:**

```cpp
#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

class StaticMesh {
public:
    bool load(const std::string& path);
    void draw(const glm::mat4& transform) const;
    bool isLoaded() const { return m_loaded; }
    const std::string& getError() const { return m_error; }

private:
    struct Vertex { glm::vec3 p, n; glm::vec2 uv; };
    std::vector<Vertex> m_vertices;
    bool m_loaded = false;
    std::string m_error;
};
```

**static_mesh.cpp:**

```cpp
#include "static_mesh.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <GL/gl.h>

bool StaticMesh::load(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

    if (!scene || !scene->HasMeshes()) {
        m_error = importer.GetErrorString();
        return false;
    }

    for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi) {
        const aiMesh* mesh = scene->mMeshes[mi];
        for (unsigned int fi = 0; fi < mesh->mNumFaces; ++fi) {
            const aiFace& face = mesh->mFaces[fi];
            if (face.mNumIndices != 3) continue;
            for (unsigned int k = 0; k < 3; ++k) {
                const unsigned int vi = face.mIndices[k];
                Vertex v;
                v.p = { mesh->mVertices[vi].x, mesh->mVertices[vi].y, mesh->mVertices[vi].z };
                v.n = mesh->HasNormals()
                    ? glm::vec3(mesh->mNormals[vi].x, mesh->mNormals[vi].y, mesh->mNormals[vi].z)
                    : glm::vec3(0, 1, 0);
                v.uv = mesh->HasTextureCoords(0)
                    ? glm::vec2(mesh->mTextureCoords[0][vi].x, mesh->mTextureCoords[0][vi].y)
                    : glm::vec2(0);
                m_vertices.push_back(v);
            }
        }
    }

    m_loaded = true;
    return true;
}

void StaticMesh::draw(const glm::mat4& transform) const {
    if (!m_loaded) return;
    glPushMatrix();
    glMultMatrixf(&transform[0][0]); // glm and GL are both column-major
    glBegin(GL_TRIANGLES);
    for (const auto& v : m_vertices) {
        glNormal3f(v.n.x, v.n.y, v.n.z);
        glVertex3f(v.p.x, v.p.y, v.p.z);
    }
    glEnd();
    glPopMatrix();
}
```

Then in your render loop:

```cpp
MapData map;
std::string err;
if (!loadMap("data/maps/map.json", map, err)) {
    // handle
}

// Cache one mesh per unique hash so shared meshes load once.
std::unordered_map<uint64_t, StaticMesh> meshes;
for (const auto& inst : map.instances) {
    if (meshes.find(inst.meshHash) == meshes.end()) {
        StaticMesh m;
        m.load(inst.meshPath); // resolve the path however your project does
        meshes.emplace(inst.meshHash, std::move(m));
    }
}

// Draw.
for (const auto& inst : map.instances) {
    auto it = meshes.find(inst.meshHash);
    if (it == meshes.end() || !it->second.isLoaded()) continue;

    glm::mat4 m(1.0f);
    m = glm::translate(m, inst.position);
    m = glm::rotate(m, glm::radians(inst.rotation.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(inst.rotation.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(inst.rotation.z), glm::vec3(0, 0, 1));
    m = glm::scale(m, inst.scale);

    it->second.draw(m);
}
```

## Common pitfalls

- **The scale gap.** If your engine's world is not authored in meters – for example,
  if a character is 60 units tall instead of 1.8 – you'll need a scale factor
  between map space and world space. Multiply both position and scale by the
  same factor:

  ```cpp
  constexpr float kMapToWorld = /* your world's units per meter */;
  m = glm::translate(m, inst.position * kMapToWorld);
  // ...
  m = glm::scale(m, inst.scale * kMapToWorld);
  ```

- **Rotation order.** `T * Rx * Ry * Rz * S`, in that order, in degrees. Any other
  order will silently give you wrong angles on multi-axis rotations.

- **Path casing.** On Windows, `Assets/` and `assets/` are the same folder. On Linux,
  they aren't. Pick one casing and use it consistently across both the export
  prefix and your game's asset layout.

- **Old maps.** Files saved before `meshPath` existed don't have the field. The loader
  above handles that gracefully (empty `meshPath`, instance gets skipped at load
  time) but the instance is still in the list. If you want to skip them entirely,
  check `inst.meshPath.empty()` before pushing.

- **Reserved fields.** `materialIndex` is present in every instance and always 0
  today. Don't build features around it assuming a semantic. It exists for
  future use.

## Beyond the minimum

**Bilinear heightmap sampling.** For anything where the camera gets close to the
ground, nearest-neighbour sampling shows obvious stair-stepping. Bilinear is
about ten lines and worth doing early:

```cpp
float sampleHeight(const MapHeightmap& hm, float x, float z) {
    const float fx = (x - hm.origin.x) / hm.cell;
    const float fz = (z - hm.origin.z) / hm.cell;
    const int i0 = (int)std::floor(fx), j0 = (int)std::floor(fz);
    const int i1 = std::min(i0 + 1, hm.w - 1), j1 = std::min(j0 + 1, hm.h - 1);
    const float tx = fx - i0, tz = fz - j0;

    auto at = [&](int i, int j) {
        i = std::clamp(i, 0, hm.w - 1);
        j = std::clamp(j, 0, hm.h - 1);
        return hm.heights[(size_t)j * hm.w + i];
    };

    const float h00 = at(i0, j0), h10 = at(i1, j0);
    const float h01 = at(i0, j1), h11 = at(i1, j1);
    return glm::mix(glm::mix(h00, h10, tx), glm::mix(h01, h11, tx), tz);
}
```

**Streaming large maps.** If a map has thousands of instances, don't load every
mesh up front. Group instances by `meshHash`, decide your stream-in radius, and
load meshes on demand as the camera moves.

**Editor round-trip.** If you want to write maps back out for the editor to
reopen, mirror the exact format – every field the loader ignores, the editor
probably uses. The safest way to learn the full shape is to place one of each
kind of object in Map-Maker and inspect the JSON.

**Instancing.** If your renderer supports instanced draws, sort instances by
`meshHash` and emit one draw call per mesh instead of one per instance. For a
map with 200 copies of the same wall segment, that's 200x fewer draw calls.

## Reference

- `Save/map.json` in the repo – a real file to compare against
- `src/Save/SaveIO.cpp` – the writer. If something in this doc disagrees with
  the code, the code wins; file a bug against the doc.