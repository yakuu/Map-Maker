#include "AssetImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>

namespace AssetImporter {

bool loadMesh(const std::string& path,
              std::vector<Vertex>& vertsOut,
              std::vector<uint32_t>& indicesOut,
              std::string& errOut) {
    Assimp::Importer imp;
    const aiScene* scene = imp.ReadFile(path.c_str(),
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs |
        aiProcess_PreTransformVertices);

    if (!scene) {
        errOut = imp.GetErrorString();
        return false;
    }
    if (!scene->HasMeshes() || scene->mNumMeshes == 0) {
        errOut = "no meshes in file";
        return false;
    }

    vertsOut.clear();
    indicesOut.clear();

    const aiMesh* m = scene->mMeshes[0];
    vertsOut.reserve(m->mNumVertices);

    for (unsigned i = 0; i < m->mNumVertices; ++i) {
        Vertex v{};
        v.position = { m->mVertices[i].x, m->mVertices[i].y, m->mVertices[i].z };
        if (m->HasNormals())
            v.normal = { m->mNormals[i].x, m->mNormals[i].y, m->mNormals[i].z };
        else
            v.normal = { 0, 1, 0 };
        if (m->HasTextureCoords(0))
            v.uv = { m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y };
        else
            v.uv = { 0, 0 };
        vertsOut.push_back(v);
    }

    for (unsigned f = 0; f < m->mNumFaces; ++f) {
        const aiFace& face = m->mFaces[f];
        if (face.mNumIndices != 3) continue;
        indicesOut.push_back(face.mIndices[0]);
        indicesOut.push_back(face.mIndices[1]);
        indicesOut.push_back(face.mIndices[2]);
    }

    if (indicesOut.empty()) {
        errOut = "no triangle faces";
        return false;
    }
    return true;
}

bool loadMeshInto(const std::string& path, Mesh& outMesh, std::string& errOut) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    if (!loadMesh(path, verts, idx, errOut)) return false;
    outMesh.upload(verts, idx);
    // name is the file stem
    std::string stem = path;
    auto slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    outMesh.name = stem;
    return true;
}

}