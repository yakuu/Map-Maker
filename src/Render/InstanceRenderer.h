#pragma once
#include "GL.h"
#include "Shader.h"
#include "Mesh.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstddef>

struct InstanceGPU {
    glm::mat4 model;
    glm::vec4 tint;
};

class InstanceRenderer {
public:
    bool init();
    void shutdown();

    void begin(const glm::mat4& viewProj);
    void submit(size_t meshHash, const glm::mat4& model, const glm::vec4& tint);
    void end();

    bool hasMesh(size_t hash) const { return meshes.find(hash) != meshes.end(); }
    Mesh& ensureMesh(size_t hash, Mesh mesh);
    void  setMesh(size_t hash, Mesh mesh);
    Mesh* getMesh(size_t hash);

    int lastDrawCalls = 0;
    int lastInstanceCount = 0;

private:
    Shader shader;
    GLuint ssbo = 0;
    size_t ssboCapacity = 0;

    std::unordered_map<size_t, Mesh> meshes;
    std::unordered_map<size_t, std::vector<InstanceGPU>> batches;
    std::vector<InstanceGPU> staging;
    glm::mat4 vp{1.0f};
};