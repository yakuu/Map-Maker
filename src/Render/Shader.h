#pragma once
#include "GL.h"
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    Shader() = default;
    ~Shader();

    bool compile(const char* vertexSrc, const char* fragmentSrc);
    void destroy();
    void bind() const;

    GLint uniform(const char* name) const;
    void setMat4(const char* name, const glm::mat4& m) const;
    void setFloat(const char* name, float v) const;
    void setInt(const char* name, int v) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setVec4(const char* name, const glm::vec4& v) const;

    GLuint program() const { return prog; }
    bool valid() const { return prog != 0; }

private:
    GLuint prog = 0;
};