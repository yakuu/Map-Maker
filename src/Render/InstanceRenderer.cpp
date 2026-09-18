#include "InstanceRenderer.h"
#include <cstdio>

static const char* kVS = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

struct Instance { mat4 model; vec4 tint; };
layout(std430, binding = 0) readonly buffer InstanceBuf {
    Instance instances[];
};

uniform mat4 uViewProj;
uniform int  uBaseInstance;

out vec3 vNormal;
out vec2 vUV;
out vec4 vTint;

void main() {
    Instance I = instances[gl_InstanceID + uBaseInstance];
    vec4 world = I.model * vec4(aPos, 1.0);
    vNormal = mat3(I.model) * aNormal;
    vUV = aUV;
    vTint = I.tint;
    gl_Position = uViewProj * world;
}
)GLSL";

static const char* kFS = R"GLSL(
#version 460 core
in vec3 vNormal;
in vec2 vUV;
in vec4 vTint;
out vec4 FragColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 l = normalize(vec3(0.4, 0.85, 0.6));
    float d = max(dot(n, l), 0.0) * 0.75 + 0.25;
    FragColor = vec4(vec3(d) * vTint.rgb, vTint.a);
}
)GLSL";

bool InstanceRenderer::init() {
    if (!shader.compile(kVS, kFS)) return false;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    ssboCapacity = 1024;
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(ssboCapacity * sizeof(InstanceGPU)),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return true;
}

void InstanceRenderer::shutdown() {
    for (auto& [k, m] : meshes) m.destroy();
    meshes.clear();
    batches.clear();
    staging.clear();
    if (ssbo) { glDeleteBuffers(1, &ssbo); ssbo = 0; }
    shader.destroy();
}

Mesh& InstanceRenderer::ensureMesh(size_t hash, Mesh mesh) {
    auto it = meshes.find(hash);
    if (it != meshes.end()) return it->second;
    mesh.name = mesh.name;
    return meshes.emplace(hash, std::move(mesh)).first->second;
}

Mesh* InstanceRenderer::getMesh(size_t hash) {
    auto it = meshes.find(hash);
    return it == meshes.end() ? nullptr : &it->second;
}

void InstanceRenderer::begin(const glm::mat4& viewProj) {
    vp = viewProj;
    for (auto& [k, v] : batches) v.clear();
    staging.clear();
}

void InstanceRenderer::submit(size_t meshHash, const glm::mat4& model,
                              const glm::vec4& tint) {
    batches[meshHash].push_back({ model, tint });
}

void InstanceRenderer::end() {
    lastDrawCalls = 0;
    lastInstanceCount = 0;

    size_t total = 0;
    for (auto& [k, v] : batches) total += v.size();
    if (total == 0) return;

    staging.reserve(total);
    for (auto& [k, v] : batches)
        for (auto& inst : v) staging.push_back(inst);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    if (total > ssboCapacity) {
        ssboCapacity = total * 2;
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     (GLsizeiptr)(ssboCapacity * sizeof(InstanceGPU)),
                     nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    (GLsizeiptr)(total * sizeof(InstanceGPU)),
                    staging.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    shader.bind();
    shader.setMat4("uViewProj", vp);

    int base = 0;
    for (auto& [hash, vec] : batches) {
        if (vec.empty()) continue;
        Mesh* m = getMesh(hash);
        if (!m || !m->vao) continue;

        glBindVertexArray(m->vao);
        shader.setInt("uBaseInstance", base);
        glDrawElementsInstanced(GL_TRIANGLES, m->indexCount,
                                GL_UNSIGNED_INT, nullptr,
                                (GLsizei)vec.size());
        glBindVertexArray(0);

        base += (int)vec.size();
        lastDrawCalls += 1;
        lastInstanceCount += (int)vec.size();
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void InstanceRenderer::setMesh(size_t hash, Mesh mesh) {
    meshes[hash] = std::move(mesh);
}