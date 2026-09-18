#include "Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstdio>

namespace {
GLuint CompileStage(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] compile error:\n%s\n", log.data());
        glDeleteShader(s);
        return 0;
    }
    return s;
}
}

Shader::~Shader() { destroy(); }

bool Shader::compile(const char* vs, const char* fs) {
    destroy();
    GLuint v = CompileStage(GL_VERTEX_SHADER, vs);
    if (!v) return false;
    GLuint f = CompileStage(GL_FRAGMENT_SHADER, fs);
    if (!f) { glDeleteShader(v); return false; }

    prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] link error:\n%s\n", log.data());
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return prog != 0;
}

void Shader::destroy() {
    if (prog) { glDeleteProgram(prog); prog = 0; }
}

void Shader::bind() const { glUseProgram(prog); }

GLint Shader::uniform(const char* n) const { return glGetUniformLocation(prog, n); }
void Shader::setMat4(const char* n, const glm::mat4& m) const {
    glUniformMatrix4fv(uniform(n), 1, GL_FALSE, glm::value_ptr(m));
}
void Shader::setFloat(const char* n, float v) const { glUniform1f(uniform(n), v); }
void Shader::setInt(const char* n, int v) const { glUniform1i(uniform(n), v); }
void Shader::setVec3(const char* n, const glm::vec3& v) const {
    glUniform3f(uniform(n), v.x, v.y, v.z);
}
void Shader::setVec4(const char* n, const glm::vec4& v) const {
    glUniform4f(uniform(n), v.x, v.y, v.z, v.w);
}