#pragma once
#include "Mesh.h"
#include <string>
#include <vector>
#include <cstdint>

namespace AssetImporter {

// Loads the first mesh of the first node in the scene found at `path`.
// Appends vertices/indices. Returns false and sets `errOut` on failure.
bool loadMesh(const std::string& path,
              std::vector<Vertex>& vertsOut,
              std::vector<uint32_t>& indicesOut,
              std::string& errOut);

// Convenience: produce a ready-to-upload Mesh from a file.
bool loadMeshInto(const std::string& path, Mesh& outMesh, std::string& errOut);

}