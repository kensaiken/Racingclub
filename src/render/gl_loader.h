#pragma once
// Minimal OpenGL 3.3 core function loader (via SDL_GL_GetProcAddress).
#include <cstddef>
#include <cstdint>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TRIANGLES 0x0004
#define GL_LEQUAL 0x0203
#define GL_LESS 0x0201
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_ZERO 0
#define GL_FRONT_AND_BACK 0x0408
#define GL_BACK 0x0405
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_RGBA 0x1908
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_RGBA8 0x8058
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#define GL_MULTISAMPLE 0x809D
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER_SRGB 0x8DB9

#define GTI_GL_FUNCS(X) \
    X(void, glClear, GLbitfield) \
    X(void, glClearColor, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(void, glViewport, GLint, GLint, GLsizei, GLsizei) \
    X(void, glEnable, GLenum) \
    X(void, glDisable, GLenum) \
    X(void, glBlendFunc, GLenum, GLenum) \
    X(void, glDepthMask, GLboolean) \
    X(void, glDepthFunc, GLenum) \
    X(void, glCullFace, GLenum) \
    X(void, glPolygonMode, GLenum, GLenum) \
    X(void, glGenTextures, GLsizei, GLuint*) \
    X(void, glDeleteTextures, GLsizei, const GLuint*) \
    X(void, glBindTexture, GLenum, GLuint) \
    X(void, glTexParameteri, GLenum, GLenum, GLint) \
    X(void, glTexParameterf, GLenum, GLenum, GLfloat) \
    X(void, glTexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) \
    X(void, glCompressedTexImage2D, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*) \
    X(void, glGenerateMipmap, GLenum) \
    X(void, glActiveTexture, GLenum) \
    X(void, glDrawElements, GLenum, GLsizei, GLenum, const void*) \
    X(void, glGenVertexArrays, GLsizei, GLuint*) \
    X(void, glDeleteVertexArrays, GLsizei, const GLuint*) \
    X(void, glBindVertexArray, GLuint) \
    X(void, glGenBuffers, GLsizei, GLuint*) \
    X(void, glDeleteBuffers, GLsizei, const GLuint*) \
    X(void, glBindBuffer, GLenum, GLuint) \
    X(void, glBufferData, GLenum, GLsizeiptr, const void*, GLenum) \
    X(void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) \
    X(void, glEnableVertexAttribArray, GLuint) \
    X(void, glDisableVertexAttribArray, GLuint) \
    X(void, glVertexAttrib4f, GLuint, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(GLuint, glCreateShader, GLenum) \
    X(void, glShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*) \
    X(void, glCompileShader, GLuint) \
    X(void, glGetShaderiv, GLuint, GLenum, GLint*) \
    X(void, glGetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar*) \
    X(void, glDeleteShader, GLuint) \
    X(GLuint, glCreateProgram, void) \
    X(void, glAttachShader, GLuint, GLuint) \
    X(void, glBindAttribLocation, GLuint, GLuint, const GLchar*) \
    X(void, glLinkProgram, GLuint) \
    X(void, glGetProgramiv, GLuint, GLenum, GLint*) \
    X(void, glGetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar*) \
    X(void, glUseProgram, GLuint) \
    X(GLint, glGetUniformLocation, GLuint, const GLchar*) \
    X(void, glUniform1i, GLint, GLint) \
    X(void, glUniform1f, GLint, GLfloat) \
    X(void, glUniform3f, GLint, GLfloat, GLfloat, GLfloat) \
    X(void, glUniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(void, glUniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(void, glReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) \
    X(void, glFinish, void)

#define GTI_GL_DECLARE(ret, name, ...) typedef ret (*PFN_##name)(__VA_ARGS__); extern PFN_##name name;
GTI_GL_FUNCS(GTI_GL_DECLARE)
#undef GTI_GL_DECLARE

namespace gti::gl {
bool load(); // call after creating the GL context
}
