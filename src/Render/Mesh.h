#pragma once
#include "GL.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept;
    Mesh& operator=(Mesh&&) noexcept;

    void upload(const std::vector<Vertex>& verts,
                const std::vector<uint32_t>& indices);
    void destroy();
    void draw() const;

    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    std::string name;
};

namespace MeshFactory {
    Mesh makeCube(float size = 1.0f);
    Mesh makePlane(float size = 2.0f, float y = 0.0f);
}