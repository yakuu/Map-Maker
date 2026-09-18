#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <cstddef>

#ifndef APIENTRY
#define APIENTRY __stdcall
#endif

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

/* ---- constants not in the Win32 GL 1.1 header ---- */
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

/* ---- function pointer typedefs ---- */
typedef void  (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void  (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void  (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void  (APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum, GLintptr, GLsizeiptr, const void*);
typedef void  (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void  (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void  (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void  (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
typedef void  (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void  (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void  (APIENTRY *PFNGLVERTEXATTRIBDIVISORPROC)(GLuint, GLuint);
typedef void  (APIENTRY *PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum, GLsizei, GLenum, const void*, GLsizei);
typedef GLuint(APIENTRY *PFNGLCREATESHADERPROC)(GLenum);
typedef void  (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void  (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint);
typedef void  (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void  (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void  (APIENTRY *PFNGLDELETESHADERPROC)(GLuint);
typedef GLuint(APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void  (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void  (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint);
typedef void  (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void  (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void  (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint);
typedef void  (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
typedef void  (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void  (APIENTRY *PFNGLUNIFORM1FPROC)(GLint, GLfloat);
typedef void  (APIENTRY *PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void  (APIENTRY *PFNGLUNIFORM3FPROC)(GLint, GLfloat, GLfloat, GLfloat);
typedef void  (APIENTRY *PFNGLUNIFORM4FPROC)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void  (APIENTRY *PFNGLBINDBUFFERBASEPROC)(GLenum, GLuint, GLuint);
typedef void  (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum);
typedef void  (APIENTRY *PFNGLGENFRAMEBUFFERSPROC)(GLsizei, GLuint*);
typedef void  (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
typedef void  (APIENTRY *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum(APIENTRY *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum);
typedef void  (APIENTRY *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void  (APIENTRY *PFNGLGENRENDERBUFFERSPROC)(GLsizei, GLuint*);
typedef void  (APIENTRY *PFNGLBINDRENDERBUFFERPROC)(GLenum, GLuint);
typedef void  (APIENTRY *PFNGLRENDERBUFFERSTORAGEPROC)(GLenum, GLenum, GLsizei, GLsizei);
typedef void  (APIENTRY *PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum, GLenum, GLenum, GLuint);
typedef void  (APIENTRY *PFNGLDELETERENDERBUFFERSPROC)(GLsizei, const GLuint*);

/* ---- function pointers ---- */
extern PFNGLGENBUFFERSPROC                 glGenBuffers;
extern PFNGLBINDBUFFERPROC                 glBindBuffer;
extern PFNGLBUFFERDATAPROC                 glBufferData;
extern PFNGLBUFFERSUBDATAPROC              glBufferSubData;
extern PFNGLDELETEBUFFERSPROC              glDeleteBuffers;
extern PFNGLGENVERTEXARRAYSPROC            glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC            glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC         glDeleteVertexArrays;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC    glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC        glVertexAttribPointer;
extern PFNGLVERTEXATTRIBDIVISORPROC        glVertexAttribDivisor;
extern PFNGLDRAWELEMENTSINSTANCEDPROC      glDrawElementsInstanced;
extern PFNGLCREATESHADERPROC               glCreateShader;
extern PFNGLSHADERSOURCEPROC               glShaderSource;
extern PFNGLCOMPILESHADERPROC              glCompileShader;
extern PFNGLGETSHADERIVPROC                glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC           glGetShaderInfoLog;
extern PFNGLDELETESHADERPROC               glDeleteShader;
extern PFNGLCREATEPROGRAMPROC              glCreateProgram;
extern PFNGLATTACHSHADERPROC               glAttachShader;
extern PFNGLLINKPROGRAMPROC                glLinkProgram;
extern PFNGLGETPROGRAMIVPROC               glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC          glGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC                 glUseProgram;
extern PFNGLDELETEPROGRAMPROC              glDeleteProgram;
extern PFNGLGETUNIFORMLOCATIONPROC         glGetUniformLocation;
extern PFNGLUNIFORMMATRIX4FVPROC           glUniformMatrix4fv;
extern PFNGLUNIFORM1FPROC                  glUniform1f;
extern PFNGLUNIFORM1IPROC                  glUniform1i;
extern PFNGLUNIFORM3FPROC                  glUniform3f;
extern PFNGLUNIFORM4FPROC                  glUniform4f;
extern PFNGLBINDBUFFERBASEPROC             glBindBufferBase;
extern PFNGLACTIVETEXTUREPROC              glActiveTexture;
extern PFNGLGENFRAMEBUFFERSPROC            glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC            glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC       glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC     glCheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC         glDeleteFramebuffers;
extern PFNGLGENRENDERBUFFERSPROC           glGenRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC           glBindRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC        glRenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC    glFramebufferRenderbuffer;
extern PFNGLDELETERENDERBUFFERSPROC        glDeleteRenderbuffers;

bool LoadGLFunctions();
const char* LastGLLoadError();