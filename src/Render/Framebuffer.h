#pragma once
#include "GL.h"

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    bool resize(int w, int h);
    void bind() const;
    void unbind() const;
    void destroy();

    GLuint colorTexture() const { return color; }
    int width() const { return w; }
    int height() const { return h; }
    bool valid() const { return fbo != 0; }

private:
    GLuint fbo = 0, color = 0, depth = 0;
    int w = 0, h = 0;
};