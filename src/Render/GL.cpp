#include "GL.h"
#include <cstdio>

PFNGLGENBUFFERSPROC              glGenBuffers              = nullptr;
PFNGLBINDBUFFERPROC              glBindBuffer              = nullptr;
PFNGLBUFFERDATAPROC              glBufferData              = nullptr;
PFNGLBUFFERSUBDATAPROC           glBufferSubData           = nullptr;
PFNGLDELETEBUFFERSPROC           glDeleteBuffers           = nullptr;
PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays         = nullptr;
PFNGLBINDVERTEXARRAYPROC         glBindVertexArray         = nullptr;
PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays      = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer     = nullptr;
PFNGLVERTEXATTRIBDIVISORPROC     glVertexAttribDivisor     = nullptr;
PFNGLDRAWELEMENTSINSTANCEDPROC   glDrawElementsInstanced   = nullptr;
PFNGLCREATESHADERPROC            glCreateShader            = nullptr;
PFNGLSHADERSOURCEPROC            glShaderSource            = nullptr;
PFNGLCOMPILESHADERPROC           glCompileShader           = nullptr;
PFNGLGETSHADERIVPROC             glGetShaderiv             = nullptr;
PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog        = nullptr;
PFNGLDELETESHADERPROC            glDeleteShader            = nullptr;
PFNGLCREATEPROGRAMPROC           glCreateProgram           = nullptr;
PFNGLATTACHSHADERPROC            glAttachShader            = nullptr;
PFNGLLINKPROGRAMPROC             glLinkProgram             = nullptr;
PFNGLGETPROGRAMIVPROC            glGetProgramiv            = nullptr;
PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog       = nullptr;
PFNGLUSEPROGRAMPROC              glUseProgram              = nullptr;
PFNGLDELETEPROGRAMPROC           glDeleteProgram           = nullptr;
PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation      = nullptr;
PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv        = nullptr;
PFNGLUNIFORM1FPROC               glUniform1f               = nullptr;
PFNGLUNIFORM1IPROC               glUniform1i               = nullptr;
PFNGLUNIFORM3FPROC               glUniform3f               = nullptr;
PFNGLUNIFORM4FPROC               glUniform4f               = nullptr;
PFNGLBINDBUFFERBASEPROC          glBindBufferBase          = nullptr;
PFNGLACTIVETEXTUREPROC           glActiveTexture           = nullptr;
PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers         = nullptr;
PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer         = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D    = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus  = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC      glDeleteFramebuffers      = nullptr;
PFNGLGENRENDERBUFFERSPROC        glGenRenderbuffers        = nullptr;
PFNGLBINDRENDERBUFFERPROC        glBindRenderbuffer        = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC     glRenderbufferStorage     = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = nullptr;
PFNGLDELETERENDERBUFFERSPROC     glDeleteRenderbuffers     = nullptr;

namespace {
char g_lastError[256] = {0};

void* GetGLProc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (!p) {
        static HMODULE mod = LoadLibraryA("opengl32.dll");
        if (mod) p = (void*)GetProcAddress(mod, name);
    }
    return p;
}

bool LoadOne(const char* name, void** out) {
    void* p = GetGLProc(name);
    if (!p) {
        std::snprintf(g_lastError, sizeof(g_lastError),
                      "Missing GL function: %s", name);
        return false;
    }
    *out = p;
    return true;
}

#define LOAD(name) if (!LoadOne(#name, (void**)&name)) return false;
}

bool LoadGLFunctions() {
    g_lastError[0] = 0;
    LOAD(glGenBuffers)
    LOAD(glBindBuffer)
    LOAD(glBufferData)
    LOAD(glBufferSubData)
    LOAD(glDeleteBuffers)
    LOAD(glGenVertexArrays)
    LOAD(glBindVertexArray)
    LOAD(glDeleteVertexArrays)
    LOAD(glEnableVertexAttribArray)
    LOAD(glVertexAttribPointer)
    LOAD(glVertexAttribDivisor)
    LOAD(glDrawElementsInstanced)
    LOAD(glCreateShader)
    LOAD(glShaderSource)
    LOAD(glCompileShader)
    LOAD(glGetShaderiv)
    LOAD(glGetShaderInfoLog)
    LOAD(glDeleteShader)
    LOAD(glCreateProgram)
    LOAD(glAttachShader)
    LOAD(glLinkProgram)
    LOAD(glGetProgramiv)
    LOAD(glGetProgramInfoLog)
    LOAD(glUseProgram)
    LOAD(glDeleteProgram)
    LOAD(glGetUniformLocation)
    LOAD(glUniformMatrix4fv)
    LOAD(glUniform1f)
    LOAD(glUniform1i)
    LOAD(glUniform3f)
    LOAD(glUniform4f)
    LOAD(glBindBufferBase)
    LOAD(glActiveTexture)
    LOAD(glGenFramebuffers)
    LOAD(glBindFramebuffer)
    LOAD(glFramebufferTexture2D)
    LOAD(glCheckFramebufferStatus)
    LOAD(glDeleteFramebuffers)
    LOAD(glGenRenderbuffers)
    LOAD(glBindRenderbuffer)
    LOAD(glRenderbufferStorage)
    LOAD(glFramebufferRenderbuffer)
    LOAD(glDeleteRenderbuffers)
    return true;
}

const char* LastGLLoadError() { return g_lastError; }