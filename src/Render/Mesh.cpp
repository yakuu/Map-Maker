#include "Mesh.h"
#include <utility>

Mesh::~Mesh() { destroy(); }

Mesh::Mesh(Mesh&& o) noexcept {
    vao = o.vao; vbo = o.vbo; ebo = o.ebo;
    indexCount = o.indexCount; name = std::move(o.name);
    o.vao = o.vbo = o.ebo = 0; o.indexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        destroy();
        vao = o.vao; vbo = o.vbo; ebo = o.ebo;
        indexCount = o.indexCount; name = std::move(o.name);
        o.vao = o.vbo = o.ebo = 0; o.indexCount = 0;
    }
    return *this;
}

void Mesh::upload(const std::vector<Vertex>& verts,
                  const std::vector<uint32_t>& indices) {
    destroy();
    indexCount = (GLsizei)indices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(Vertex)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

void Mesh::destroy() {
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    indexCount = 0;
}

void Mesh::draw() const {
    if (!vao) return;
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

namespace MeshFactory {

Mesh makeCube(float size) {
    const float h = size * 0.5f;
    std::vector<Vertex> v = {
        // +X
        {{ h,-h,-h},{ 1, 0, 0},{0,0}}, {{ h, h,-h},{ 1, 0, 0},{0,1}},
        {{ h, h, h},{ 1, 0, 0},{1,1}}, {{ h,-h, h},{ 1, 0, 0},{1,0}},
        // -X
        {{-h,-h, h},{-1, 0, 0},{0,0}}, {{-h, h, h},{-1, 0, 0},{0,1}},
        {{-h, h,-h},{-1, 0, 0},{1,1}}, {{-h,-h,-h},{-1, 0, 0},{1,0}},
        // +Y
        {{-h, h,-h},{ 0, 1, 0},{0,0}}, {{-h, h, h},{ 0, 1, 0},{0,1}},
        {{ h, h, h},{ 0, 1, 0},{1,1}}, {{ h, h,-h},{ 0, 1, 0},{1,0}},
        // -Y
        {{-h,-h, h},{ 0,-1, 0},{0,0}}, {{-h,-h,-h},{ 0,-1, 0},{0,1}},
        {{ h,-h,-h},{ 0,-1, 0},{1,1}}, {{ h,-h, h},{ 0,-1, 0},{1,0}},
        // +Z
        {{-h,-h, h},{ 0, 0, 1},{0,0}}, {{ h,-h, h},{ 0, 0, 1},{0,1}},
        {{ h, h, h},{ 0, 0, 1},{1,1}}, {{-h, h, h},{ 0, 0, 1},{1,0}},
        // -Z
        {{ h,-h,-h},{ 0, 0,-1},{0,0}}, {{-h,-h,-h},{ 0, 0,-1},{0,1}},
        {{-h, h,-h},{ 0, 0,-1},{1,1}}, {{ h, h,-h},{ 0, 0,-1},{1,0}},
    };
    std::vector<uint32_t> idx;
    idx.reserve(36);
    for (uint32_t f = 0; f < 6; ++f) {
        uint32_t b = f * 4;
        idx.insert(idx.end(), { b+0,b+1,b+2, b+0,b+2,b+3 });
    }
    Mesh m; m.upload(v, idx); m.name = "cube";
    return m;
}

Mesh makePlane(float size, float y) {
    const float h = size * 0.5f;
    std::vector<Vertex> v = {
        {{-h, y,-h},{0,1,0},{0,0}}, {{ h, y,-h},{0,1,0},{1,0}},
        {{ h, y, h},{0,1,0},{1,1}}, {{-h, y, h},{0,1,0},{0,1}},
    };
    std::vector<uint32_t> idx = { 0,1,2, 0,2,3 };
    Mesh m; m.upload(v, idx); m.name = "plane";
    return m;
}

}