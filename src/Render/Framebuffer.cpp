#include "Framebuffer.h"
#include <cstdio>

Framebuffer::~Framebuffer() { destroy(); }

void Framebuffer::destroy() {
    if (depth) { glDeleteRenderbuffers(1, &depth); depth = 0; }
    if (color) { glDeleteTextures(1, &color); color = 0; }
    if (fbo)   { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    w = h = 0;
}

bool Framebuffer::resize(int nw, int nh) {
    if (nw <= 0 || nh <= 0) return false;
    if (nw == w && nh == h && fbo) return true;
    destroy();
    w = nw; h = nh;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, color, 0);

    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, depth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[fbo] incomplete: 0x%X\n", status);
        destroy();
        return false;
    }
    return true;
}

void Framebuffer::bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }
void Framebuffer::unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }