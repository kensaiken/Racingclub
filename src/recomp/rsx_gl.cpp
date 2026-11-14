// RSX -> OpenGL 4.5 renderer (see rsx_gl.h).
#include "recomp/rsx_gl.h"
#include "recomp/settings.h"
#include "recomp/lv2.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gti::recomp { bool host_menu_bitmap(const wchar_t* const* lines, int n, int sel, int w, int h, uint32_t* out); }   // host_win.cpp
namespace gti::recomp::rsx {

// ------------------------------------------------------------------ OpenGL loader (only what this file uses)
typedef unsigned int GLenum; typedef unsigned char GLboolean; typedef unsigned int GLbitfield; typedef int GLint;
typedef int GLsizei; typedef unsigned int GLuint; typedef float GLfloat; typedef double GLdouble; typedef char GLchar;
typedef ptrdiff_t GLsizeiptr; typedef ptrdiff_t GLintptr;
enum : GLenum {
    GL_NONE = 0, GL_POINTS = 0, GL_LINES = 1, GL_LINE_LOOP = 2, GL_LINE_STRIP = 3, GL_TRIANGLES = 4, GL_TRIANGLE_STRIP = 5, GL_TRIANGLE_FAN = 6,
    GL_DEPTH_BUFFER_BIT = 0x100, GL_STENCIL_BUFFER_BIT = 0x400, GL_COLOR_BUFFER_BIT = 0x4000,
    GL_FRONT_AND_BACK = 0x408, GL_CULL_FACE = 0x0B44, GL_DEPTH_TEST = 0x0B71, GL_STENCIL_TEST = 0x0B90, GL_BLEND = 0x0BE2,
    GL_SCISSOR_TEST = 0x0C11, GL_POLYGON_OFFSET_FILL = 0x8037, GL_TEXTURE_2D = 0x0DE1, GL_TEXTURE_3D = 0x806F, GL_TEXTURE_CUBE_MAP = 0x8513,
    GL_TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515, GL_UNSIGNED_BYTE = 0x1401, GL_UNSIGNED_SHORT = 0x1403, GL_UNSIGNED_INT = 0x1405, GL_FLOAT = 0x1406,
    GL_HALF_FLOAT = 0x140B, GL_RED = 0x1903, GL_GREEN = 0x1904, GL_BLUE = 0x1905, GL_ALPHA = 0x1906, GL_RGB = 0x1907, GL_RGBA = 0x1908, GL_RG = 0x8227,
    GL_BGRA = 0x80E1, GL_DEPTH_COMPONENT = 0x1902, GL_DEPTH_STENCIL = 0x84F9, GL_UNSIGNED_INT_24_8 = 0x84FA, GL_UNSIGNED_INT_8_8_8_8 = 0x8035,
    GL_UNSIGNED_SHORT_5_6_5 = 0x8363, GL_UNSIGNED_SHORT_4_4_4_4_REV = 0x8365, GL_UNSIGNED_SHORT_1_5_5_5_REV = 0x8366,
    GL_NEAREST = 0x2600, GL_LINEAR = 0x2601, GL_NEAREST_MIPMAP_NEAREST = 0x2700, GL_LINEAR_MIPMAP_NEAREST = 0x2701,
    GL_NEAREST_MIPMAP_LINEAR = 0x2702, GL_LINEAR_MIPMAP_LINEAR = 0x2703, GL_TEXTURE_MAG_FILTER = 0x2800, GL_TEXTURE_MIN_FILTER = 0x2801,
    GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803, GL_TEXTURE_WRAP_R = 0x8072, GL_REPEAT = 0x2901, GL_CLAMP_TO_EDGE = 0x812F,
    GL_CLAMP_TO_BORDER = 0x812D, GL_MIRRORED_REPEAT = 0x8370, GL_MIRROR_CLAMP_TO_EDGE = 0x8743, GL_TEXTURE_MIN_LOD = 0x813A, GL_TEXTURE_MAX_LOD = 0x813B,
    GL_TEXTURE_BASE_LEVEL = 0x813C, GL_TEXTURE_MAX_LEVEL = 0x813D, GL_TEXTURE_SWIZZLE_RGBA = 0x8E46, GL_TEXTURE_BORDER_COLOR = 0x1004,
    GL_TEXTURE_MAX_ANISOTROPY = 0x84FE, GL_TEXTURE0 = 0x84C0, GL_ZERO = 0, GL_ONE = 1,
    GL_RGBA8 = 0x8058, GL_SRGB8_ALPHA8 = 0x8C43, GL_R8 = 0x8229, GL_RG8 = 0x822B, GL_R16 = 0x822A, GL_RG16 = 0x822C, GL_RGBA16F = 0x881A,
    GL_RGBA32F = 0x8814, GL_R32F = 0x822E, GL_RG16F = 0x822F, GL_RGB565 = 0x8D62, GL_RGBA4 = 0x8056, GL_RGB5_A1 = 0x8057,
    GL_DEPTH24_STENCIL8 = 0x88F0, GL_DEPTH_COMPONENT16 = 0x81A5,
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT = 0x83F1, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT = 0x83F2, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT = 0x83F3,
    GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT = 0x8C4D, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT = 0x8C4E, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT = 0x8C4F,
    GL_ARRAY_BUFFER = 0x8892, GL_ELEMENT_ARRAY_BUFFER = 0x8893, GL_UNIFORM_BUFFER = 0x8A11, GL_STREAM_DRAW = 0x88E0, GL_DYNAMIC_DRAW = 0x88E8,
    GL_FRAGMENT_SHADER = 0x8B30, GL_VERTEX_SHADER = 0x8B31, GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82, GL_INFO_LOG_LENGTH = 0x8B84,
    GL_FRAMEBUFFER = 0x8D40, GL_READ_FRAMEBUFFER = 0x8CA8, GL_DRAW_FRAMEBUFFER = 0x8CA9, GL_COLOR_ATTACHMENT0 = 0x8CE0,
    GL_DEPTH_ATTACHMENT = 0x8D00, GL_DEPTH_STENCIL_ATTACHMENT = 0x821A, GL_FRAMEBUFFER_COMPLETE = 0x8CD5,
    GL_LOWER_LEFT = 0x8CA1, GL_ZERO_TO_ONE = 0x935F, GL_UNPACK_ALIGNMENT = 0x0CF5, GL_UNPACK_ROW_LENGTH = 0x0CF2,
    GL_CW = 0x0900, GL_CCW = 0x0901, GL_FRAMEBUFFER_SRGB = 0x8DB9, GL_RENDERER = 0x1F01, GL_VERSION = 0x1F02, GL_DEBUG_OUTPUT = 0x92E0,
    GL_DEPTH_CLAMP = 0x864F, GL_PROGRAM_POINT_SIZE = 0x8642,
};
#define RSX_GL(X) \
    X(const unsigned char*, glGetString, GLenum) X(void, glEnable, GLenum) X(void, glDisable, GLenum) X(void, glViewport, GLint, GLint, GLsizei, GLsizei) \
    X(void, glScissor, GLint, GLint, GLsizei, GLsizei) X(void, glClear, GLbitfield) X(void, glClearColor, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(void, glClearDepth, GLdouble) X(void, glClearStencil, GLint) X(void, glColorMaski, GLuint, GLboolean, GLboolean, GLboolean, GLboolean) \
    X(void, glDepthMask, GLboolean) X(void, glDepthFunc, GLenum) X(void, glStencilFuncSeparate, GLenum, GLenum, GLint, GLuint) \
    X(void, glStencilOpSeparate, GLenum, GLenum, GLenum, GLenum) X(void, glStencilMaskSeparate, GLenum, GLuint) \
    X(void, glBlendFuncSeparate, GLenum, GLenum, GLenum, GLenum) X(void, glBlendEquationSeparate, GLenum, GLenum) \
    X(void, glBlendColor, GLfloat, GLfloat, GLfloat, GLfloat) X(void, glCullFace, GLenum) X(void, glFrontFace, GLenum) \
    X(void, glPolygonOffset, GLfloat, GLfloat) X(void, glClipControl, GLenum, GLenum) X(void, glGenTextures, GLsizei, GLuint*) \
    X(void, glDeleteTextures, GLsizei, const GLuint*) X(void, glBindTexture, GLenum, GLuint) X(void, glTexParameteri, GLenum, GLenum, GLint) \
    X(void, glTexParameterf, GLenum, GLenum, GLfloat) X(void, glTexParameteriv, GLenum, GLenum, const GLint*) X(void, glTexParameterfv, GLenum, GLenum, const GLfloat*) \
    X(void, glTexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) \
    X(void, glCompressedTexImage2D, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*) \
    X(void, glActiveTexture, GLenum) X(void, glPixelStorei, GLenum, GLint) X(void, glGenFramebuffers, GLsizei, GLuint*) \
    X(void, glBindFramebuffer, GLenum, GLuint) X(void, glFramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint) \
    X(void, glDrawBuffers, GLsizei, const GLenum*) X(GLenum, glCheckFramebufferStatus, GLenum) \
    X(void, glCopyTexSubImage2D, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei) \
    X(void, glBlitFramebuffer, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum) \
    X(void, glDeleteFramebuffers, GLsizei, const GLuint*) X(void, glGenBuffers, GLsizei, GLuint*) X(void, glBindBuffer, GLenum, GLuint) \
    X(void, glBufferData, GLenum, GLsizeiptr, const void*, GLenum) X(void, glBindBufferBase, GLenum, GLuint, GLuint) \
    X(void, glGenVertexArrays, GLsizei, GLuint*) X(void, glBindVertexArray, GLuint) \
    X(void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) X(void, glEnableVertexAttribArray, GLuint) \
    X(void, glDisableVertexAttribArray, GLuint) X(void, glVertexAttrib4f, GLuint, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(void, glDrawElements, GLenum, GLsizei, GLenum, const void*) X(void, glDrawElementsBaseVertex, GLenum, GLsizei, GLenum, const void*, GLint) X(void, glDrawArrays, GLenum, GLint, GLsizei) \
    X(GLuint, glCreateShader, GLenum) X(void, glShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*) X(void, glCompileShader, GLuint) \
    X(void, glGetShaderiv, GLuint, GLenum, GLint*) X(void, glGetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar*) X(GLuint, glCreateProgram) \
    X(void, glProgramBinary, GLuint, GLenum, const void*, GLsizei) X(void, glGetProgramBinary, GLuint, GLsizei, GLsizei*, GLenum*, void*) \
    X(void, glAttachShader, GLuint, GLuint) X(void, glLinkProgram, GLuint) X(void, glGetProgramiv, GLuint, GLenum, GLint*) \
    X(void, glGetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar*) X(void, glUseProgram, GLuint) X(GLint, glGetUniformLocation, GLuint, const GLchar*) \
    X(void, glUniform4fv, GLint, GLsizei, const GLfloat*) X(void, glUniform1i, GLint, GLint) X(void, glDeleteShader, GLuint) X(GLenum, glGetError) X(void, glReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) X(void, glGetIntegerv, GLenum, GLint*) X(GLboolean, glIsEnabled, GLenum)
#define RSX_DECL(ret, name, ...) typedef ret (*PFN_##name)(__VA_ARGS__); static PFN_##name name = nullptr;
RSX_GL(RSX_DECL)
#undef RSX_DECL

static std::mutex g_logMx;
#define RLOG(...) do { std::lock_guard<std::mutex> lk_(g_logMx); std::fprintf(stderr, __VA_ARGS__); } while (0)

// ------------------------------------------------------------------ state
static Config C;
static uint32_t R[0x4000];                     // last value written to every NV4097 method (index = method / 4)
static uint32_t vpCode[512 * 4];               // transform program memory
static float vpConst[468 * 4];                 // transform constants
static bool vpConstDirty = true;
static uint32_t vpLoad = 0, cLoad = 0;         // load cursors (in words / constants)
static uint32_t vpConstWord = 0;
struct Range { uint32_t first, count; };
static std::vector<Range> ranges;
static bool rangesIndexed = false;
static uint32_t prim = 0;
struct Display { uint32_t offset = 0, pitch = 0, w = 0, h = 0; } displays[8];

static SDL_Window* win = nullptr;
static void set_display_mode(int mode);
static int g_lastFull = 1;   // the full-screen mode F11 goes back to
static SDL_GLContext glc = nullptr;
static bool glOk = false, initTried = false;
static std::atomic<bool> quitFlag{false};
static GLuint vao = 0, vbo = 0, ibo = 0, ubo = 0;
static uint64_t frameNo = 0, drawsThisFrame = 0;
static const bool dumpShaders = std::getenv("RSXDUMP") != nullptr;
static const bool rsxTrace = std::getenv("RSXTRACE") != nullptr;

bool enabled() { static const bool on = !(std::getenv("RSX") && std::getenv("RSX")[0] == '0'); return on; }
bool quit_requested() { return quitFlag.load(); }

static inline uint32_t be32(uint32_t ea) { uint32_t v; std::memcpy(&v, C.mem + ea, 4); return __builtin_bswap32(v); }
static inline uint16_t be16(uint32_t ea) { return uint16_t(C.mem[ea] << 8 | C.mem[ea + 1]); }
static inline float f32(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }
static inline float rf(uint32_t m) { return f32(R[m >> 2]); }
static float half2f(uint16_t h) {
    const uint32_t sgn = uint32_t(h & 0x8000) << 16, e = (h >> 10) & 0x1f, m = h & 0x3ff;
    uint32_t u;
    if (e == 0) {
        if (!m) u = sgn;
        else { const float v = float(m) * (1.f / 16777216.f); std::memcpy(&u, &v, 4); u |= sgn; }   // subnormal: m * 2^-24
    } else if (e == 31) u = sgn | 0x7f800000u | (m << 13);
    else u = sgn | ((e + 112) << 23) | (m << 13);
    float f; std::memcpy(&f, &u, 4); return f;
}
static uint32_t ea_of(uint32_t offset, uint32_t location /*0 local, 1 main*/) { return (location ? C.ioAddr : C.localAddr) + offset; }

static uint64_t g_texUploads = 0, g_texUploadBytes = 0, g_streamBuilds = 0, g_streamVerts = 0, g_progBuilds = 0;   // work counters, see [slowdraw]
static uint64_t fnv(const void* p, size_t n, uint64_t h = 1469598103934665603ull) {   // keys used inside this process only
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (; n >= 8; n -= 8, b += 8) { uint64_t w; std::memcpy(&w, b, 8); h = (h ^ w) * 0x9E3779B97F4A7C15ull; h ^= h >> 29; }
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}

// ------------------------------------------------------------------ vertex program -> GLSL
// Instruction layout (4 words) as documented by the nouveau / RPCS3 projects for NV40-class vertex engines.
struct VpInfo { std::string glsl; uint32_t inputs = 0; uint32_t outputs = 0; int len = 0; bool unsupported = false; };

static const char* swz4(uint32_t x, uint32_t y, uint32_t z, uint32_t w, char* buf) {
    static const char c[] = "xyzw";
    buf[0] = '.'; buf[1] = c[x]; buf[2] = c[y]; buf[3] = c[z]; buf[4] = c[w]; buf[5] = 0;
    return buf;
}
static const char* vpOut(uint32_t d) {
    static const char* n[] = {"o_pos", "v_col0", "v_col1", "v_bfc0", "v_bfc1", "v_fog", "o_psz",
                              "v_tc[0]", "v_tc[1]", "v_tc[2]", "v_tc[3]", "v_tc[4]", "v_tc[5]", "v_tc[6]", "v_tc[7]", "v_tc[8]", "v_tc[9]"};
    return d < 17 ? n[d] : "o_dummy";
}

static VpInfo translate_vp(const uint32_t* code, uint32_t start) {
    VpInfo vi;
    std::string b;
    char t[512], sw[8];
    for (uint32_t pc = start; pc < 512; ++pc) {
        const uint32_t* in = code + pc * 4;
        const uint32_t d0 = in[0], d1 = in[1], d2 = in[2], d3 = in[3];
        ++vi.len;
        const uint32_t vecOp = (d1 >> 22) & 0x1f, scaOp = (d1 >> 27) & 0x1f;
        const uint32_t inputSrc = (d1 >> 8) & 0xf, constSrc = (d1 >> 12) & 0x3ff;
        const uint32_t src[3] = {((d1 & 0xff) << 9) | (d2 >> 23), (d2 >> 6) & 0x1ffff, ((d2 & 0x3f) << 11) | (d3 >> 21)};
        const bool absS[3] = {bool((d0 >> 21) & 1), bool((d0 >> 22) & 1), bool((d0 >> 23) & 1)};
        const bool indexConst = (d3 >> 1) & 1;
        const uint32_t addrSel = (d0 >> 24) & 1, addrSwz = d0 & 3;
        auto operand = [&](int i) -> std::string {
            const uint32_t s = src[i];
            const uint32_t type = s & 3, tmp = (s >> 2) & 0x3f;
            std::string r;
            if (type == 1) { std::snprintf(t, sizeof t, "R[%u]", tmp); r = t; }
            else if (type == 2) { vi.inputs |= 1u << inputSrc; std::snprintf(t, sizeof t, "in%u", inputSrc); r = t; }
            else if (type == 3) {
                if (indexConst) std::snprintf(t, sizeof t, "vc[clamp(%u + A%u.%c, 0, 467)]", constSrc, addrSel, "xyzw"[addrSwz]);
                else std::snprintf(t, sizeof t, "vc[%u]", constSrc);
                r = t;
            } else r = "vec4(0.0)";
            r += swz4((s >> 14) & 3, (s >> 12) & 3, (s >> 10) & 3, (s >> 8) & 3, sw);
            if (absS[i]) r = "abs(" + r + ")";
            if ((s >> 16) & 1) r = "(-" + r + ")";
            return r;
        };
        const std::string a = operand(0), bb = operand(1), c = operand(2);
        std::string vexpr, sexpr;
        switch (vecOp) {
            case 0: break;
            case 1: vexpr = a; break;
            case 2: vexpr = a + "*" + bb; break;
            case 3: vexpr = a + "+" + c; break;
            case 4: vexpr = a + "*" + bb + "+" + c; break;
            case 5: vexpr = "vec4(dot((" + a + ").xyz,(" + bb + ").xyz))"; break;
            case 6: vexpr = "vec4(dot((" + a + ").xyz,(" + bb + ").xyz)+(" + bb + ").w)"; break;
            case 7: vexpr = "vec4(dot(" + a + "," + bb + "))"; break;
            case 8: vexpr = "vec4(1.0,(" + a + ").y*(" + bb + ").y,(" + a + ").z,(" + bb + ").w)"; break;
            case 9: vexpr = "min(" + a + "," + bb + ")"; break;
            case 10: vexpr = "max(" + a + "," + bb + ")"; break;
            case 11: vexpr = "vec4(lessThan(" + a + "," + bb + "))"; break;
            case 12: vexpr = "vec4(greaterThanEqual(" + a + "," + bb + "))"; break;
            case 13: vexpr = "ARL"; break;
            case 14: vexpr = "fract(" + a + ")"; break;
            case 15: vexpr = "floor(" + a + ")"; break;
            case 16: vexpr = "vec4(equal(" + a + "," + bb + "))"; break;
            case 17: vexpr = "vec4(0.0)"; break;
            case 18: vexpr = "vec4(greaterThan(" + a + "," + bb + "))"; break;
            case 19: vexpr = "vec4(lessThanEqual(" + a + "," + bb + "))"; break;
            case 20: vexpr = "vec4(notEqual(" + a + "," + bb + "))"; break;
            case 21: vexpr = "vec4(1.0)"; break;
            case 22: vexpr = "sign(" + a + ")"; break;
            case 25: vexpr = "vec4(0.0)"; break;   // TXL (vertex texture fetch): not used by this game so far
            default: vi.unsupported = true; std::snprintf(t, sizeof t, "  // unsupported vector op %u\n", vecOp); b += t; break;
        }
        const std::string s = "(" + c + ").x";
        switch (scaOp) {
            case 0: break;
            case 1: sexpr = "vec4(" + s + ")"; break;
            case 2: sexpr = "vec4(1.0/" + s + ")"; break;
            case 3: sexpr = "vec4(rcc(" + s + "))"; break;
            case 4: sexpr = "vec4(inversesqrt(abs(" + s + ")))"; break;
            case 5: sexpr = "vexp(" + s + ")"; break;
            case 6: sexpr = "vlog(" + s + ")"; break;
            case 7: sexpr = "vlit(" + c + ")"; break;
            case 13: sexpr = "vec4(log2(abs(" + s + ")))"; break;
            case 14: sexpr = "vec4(exp2(" + s + "))"; break;
            case 15: sexpr = "vec4(sin(" + s + "))"; break;
            case 16: sexpr = "vec4(cos(" + s + "))"; break;
            default: vi.unsupported = true; std::snprintf(t, sizeof t, "  // unsupported scalar op %u (flow control)\n", scaOp); b += t; break;
        }
        // condition (test against a condition register) shared by both units
        const bool condTest = (d0 >> 13) & 1;
        const uint32_t cond = (d0 >> 10) & 7;
        const bool condUpd = ((d0 >> 14) & 1) || ((d0 >> 29) & 1);
        const uint32_t ccSel = (d0 >> 25) & 1;
        const uint32_t cswz[4] = {(d0 >> 8) & 3, (d0 >> 6) & 3, (d0 >> 4) & 3, (d0 >> 2) & 3};
        const bool sat = (d0 >> 26) & 1;
        const uint32_t dst = (d3 >> 2) & 0x1f, dstTmp = (d0 >> 15) & 0x3f, scaTmp = (d3 >> 7) & 0x3f;
        const bool vecResult = (d0 >> 30) & 1;
        const bool vm[4] = {bool((d3 >> 16) & 1), bool((d3 >> 15) & 1), bool((d3 >> 14) & 1), bool((d3 >> 13) & 1)};
        const bool sm[4] = {bool((d3 >> 20) & 1), bool((d3 >> 19) & 1), bool((d3 >> 18) & 1), bool((d3 >> 17) & 1)};
        if (dst != 0x1f && dst < 17) vi.outputs |= 1u << dst;
        auto emit = [&](const std::string& value, const bool* mask, std::vector<std::string> targets) {
            if (condTest && cond == 0) return;
            std::string v = sat ? "clamp(" + value + ",0.0,1.0)" : value;
            b += "  t = " + v + ";\n";
            if (condUpd) { std::snprintf(t, sizeof t, "  CC%u = t;\n", ccSel); b += t; }   // (writes all components)
            for (const auto& tg : targets)
                for (int k = 0; k < 4; ++k) {
                    if (!mask[k]) continue;
                    const char comp = "xyzw"[k];
                    if (condTest && cond != 7) {
                        static const char* ops[] = {"false", "<", "==", "<=", ">", "!=", ">=", "true"};
                        std::snprintf(t, sizeof t, "  if (CC%u.%c %s 0.0) %s.%c = t.%c;\n", ccSel, "xyzw"[cswz[k]], ops[cond], tg.c_str(), comp, comp);
                    } else std::snprintf(t, sizeof t, "  %s.%c = t.%c;\n", tg.c_str(), comp, comp);
                    b += t;
                }
        };
        if (!vexpr.empty()) {
            if (vexpr == "ARL") {
                std::snprintf(t, sizeof t, "  A%u = ivec4(floor(%s));\n", addrSel, a.c_str());
                b += t;
            } else {
                std::vector<std::string> tg;
                if (dstTmp != 0x3f) { std::snprintf(t, sizeof t, "R[%u]", dstTmp); tg.push_back(t); }
                if (dst != 0x1f && (vecResult || scaOp == 0)) tg.push_back(vpOut(dst));
                emit(vexpr, vm, tg);
            }
        }
        if (!sexpr.empty()) {
            std::vector<std::string> tg;
            if (scaTmp != 0x3f) { std::snprintf(t, sizeof t, "R[%u]", scaTmp); tg.push_back(t); }
            else if (dst != 0x1f && (!vecResult || vecOp == 0)) tg.push_back(vpOut(dst));
            emit(sexpr, sm, tg);
        }
        if (d3 & 1) break;   // end of program
    }
    std::string h = "#version 450\n"
                    "layout(std140, binding = 0) uniform VC { vec4 vc[468]; };\n"
                    "uniform vec4 u_vscale, u_voffset, u_surf;\n";
    for (int k = 0; k < 16; ++k)
        if (vi.inputs & (1u << k)) { std::snprintf(t, sizeof t, "layout(location = %d) in vec4 in%d;\n", k, k); h += t; }
    h += "out vec4 v_col0, v_col1, v_bfc0, v_bfc1, v_fog;\nout vec4 v_tc[10];\n"
         "float rcc(float x) { float r = 1.0 / x; return sign(r) * clamp(abs(r), 5.42101e-20, 1.884467e19); }\n"
         "vec4 vexp(float x) { float f = floor(x); return vec4(exp2(f), x - f, exp2(x), 1.0); }\n"
         "vec4 vlog(float x) { float a = abs(x); float e = floor(log2(a)); return vec4(e, a / exp2(e), log2(a), 1.0); }\n"
         "vec4 vlit(vec4 s) { float d = max(s.x, 0.0); float sp = s.x > 0.0 ? pow(max(s.y, 0.0), clamp(s.w, -127.9961, 127.9961)) : 0.0; return vec4(1.0, d, sp, 1.0); }\n"
         "void main() {\n"
         "  vec4 R[64]; for (int i = 0; i < 64; ++i) R[i] = vec4(0.0);\n"
         "  vec4 t, CC0 = vec4(0.0), CC1 = vec4(0.0); ivec4 A0 = ivec4(0), A1 = ivec4(0);\n"
         "  vec4 o_pos = vec4(0.0, 0.0, 0.0, 1.0), o_psz = vec4(1.0), o_dummy;\n"
         "  v_col0 = vec4(1.0); v_col1 = vec4(0.0); v_bfc0 = vec4(1.0); v_bfc1 = vec4(0.0); v_fog = vec4(0.0);\n"
         "  for (int i = 0; i < 10; ++i) v_tc[i] = vec4(0.0, 0.0, 0.0, 1.0);\n";
    // epilogue: the RSX viewport transform (window = ndc * scale + offset, window y down from the top of the
    // surface) folded into clip space so that GL clipping still works; FBO row 0 = surface memory row 0.
    if (std::getenv("RSXW1")) b += "  o_pos.w = 1.0;\n";
    if (std::getenv("RSXVCDBG")) b += "  v_col0 = vc[467]; v_col1 = vc[4];\n";
    vi.glsl = h + b +
              "  gl_Position = vec4(o_pos.x * (2.0 * u_vscale.x / u_surf.x) + o_pos.w * (2.0 * u_voffset.x / u_surf.x - 1.0),\n"
              "                     o_pos.y * (2.0 * u_vscale.y / u_surf.y) + o_pos.w * (2.0 * u_voffset.y / u_surf.y - 1.0),\n"
              "                     o_pos.z * u_vscale.z + o_pos.w * u_voffset.z, o_pos.w);\n"
              "  gl_PointSize = o_psz.x;\n" +
              std::string(std::getenv("RSXVSDBG") && (vi.inputs & 1) ? "  v_col0 = vec4(gl_Position.x * 0.25 + 0.5, gl_Position.y * 0.25 + 0.5, gl_Position.w * 0.25, gl_Position.z * 0.25 + 0.5); gl_Position = vec4(float(gl_VertexID & 1) * 2.0 - 1.0, float(gl_VertexID & 2) - 1.0, 0.5, 1.0);\n" : "") +
              "}\n";
    return vi;
}

// ------------------------------------------------------------------ fragment program -> GLSL
struct FpInfo { std::string glsl; uint32_t texUsed = 0; uint32_t size = 0; uint32_t nConst = 0; bool unsupported = false; bool writesR0 = false, writesH0 = false; };
static inline uint32_t fpw(uint32_t ea) { const uint32_t v = be32(ea); return (v << 16) | (v >> 16); }

static uint32_t fp_size(uint32_t ea) {   // bytes up to and including the instruction with the end bit
    uint32_t o = 0;
    for (int n = 0; n < 2048; ++n) {
        const uint32_t dst = fpw(ea + o), s0 = fpw(ea + o + 4), s1 = fpw(ea + o + 8), s2 = fpw(ea + o + 12);
        o += 16;
        if (((s0 & 3) == 2) || ((s1 & 3) == 2) || ((s2 & 3) == 2)) o += 16;   // inline constant
        if (dst & 1) break;
    }
    return o;
}

// walks the program: hashes the instruction words only (the inline constants hold the uniform values the game
// patches into the microcode) and optionally collects the constants in program order
static uint64_t fp_walk(uint32_t ea, uint64_t h, std::vector<float>* consts) {
    uint32_t o = 0;
    for (int n = 0; n < 2048; ++n) {
        uint32_t w[4];
        for (int k = 0; k < 4; ++k) w[k] = fpw(ea + o + 4 * k);
        h = fnv(w, 16, h);
        o += 16;
        if (((w[1] & 3) == 2) || ((w[2] & 3) == 2) || ((w[3] & 3) == 2)) {
            if (consts) for (int k = 0; k < 4; ++k) consts->push_back(f32(fpw(ea + o + 4 * k)));
            o += 16;
        }
        if (w[0] & 1) break;
    }
    return h;
}

// texDims: 2 bits per sampler: 0 = 2D, 1 = cube, 2 = 3D, 3 = 1D; texUn: unnormalised coordinates
static FpInfo translate_fp(uint32_t ea, uint32_t texDims, uint32_t control) {
    FpInfo fi;
    std::string b;
    char t[640], sw[8];
    uint32_t o = 0;
    std::vector<uint32_t> elseAt, endAt;   // IFE blocks (word offsets)
    for (int n = 0; n < 2048; ++n) {
        while (!elseAt.empty() && elseAt.back() == o / 4) { b += "  } else {\n"; elseAt.pop_back(); }
        while (!endAt.empty() && endAt.back() == o / 4) { b += "  }\n"; endAt.pop_back(); }
        const uint32_t dst = fpw(ea + o), s0 = fpw(ea + o + 4), s1 = fpw(ea + o + 8), s2 = fpw(ea + o + 12);
        const bool hasConst = ((s0 & 3) == 2) || ((s1 & 3) == 2) || ((s2 & 3) == 2);
        const uint32_t constIdx = fi.nConst;
        if (hasConst) ++fi.nConst;
        const uint32_t opcode = ((dst >> 24) & 0x3f) | (((s1 >> 31) & 1) << 6);
        const uint32_t destReg = (dst >> 1) & 0x3f, attr = (dst >> 13) & 0xf, texNum = (dst >> 17) & 0xf;
        const bool fp16 = (dst >> 7) & 1, setCond = (dst >> 8) & 1, noDest = (dst >> 30) & 1, sat = (dst >> 31) & 1, bx2 = (dst >> 21) & 1;
        const uint32_t prec = (dst >> 22) & 3;
        const bool mask[4] = {bool((dst >> 9) & 1), bool((dst >> 10) & 1), bool((dst >> 11) & 1), bool((dst >> 12) & 1)};
        auto operand = [&](uint32_t s, bool absBit) -> std::string {
            const uint32_t type = s & 3, idx = (s >> 2) & 0x3f;
            const bool h = (s >> 8) & 1;
            std::string r;
            if (type == 0) { std::snprintf(t, sizeof t, "%s[%u]", h ? "H" : "R", idx); r = t; }
            else if (type == 1) {
                static const char* in[] = {"wpos", "v_col0", "v_col1", "v_fog", "v_tc[0]", "v_tc[1]", "v_tc[2]", "v_tc[3]", "v_tc[4]", "v_tc[5]",
                                           "v_tc[6]", "v_tc[7]", "v_tc[8]", "v_tc[9]", "ssa", "vec4(0.0)"};
                r = in[attr];
                if (attr == 1 || attr == 2) r = attr == 1 ? "col0()" : "col1()";
            } else if (type == 2) {
                std::snprintf(t, sizeof t, "fc[%u]", constIdx); r = t;
            }
            else r = "vec4(0.0)";
            r += swz4((s >> 9) & 3, (s >> 11) & 3, (s >> 13) & 3, (s >> 15) & 3, sw);
            if (absBit) r = "abs(" + r + ")";
            if ((s >> 17) & 1) r = "(-" + r + ")";
            return r;
        };
        const std::string a = operand(s0, (s0 >> 29) & 1), bb = operand(s1, (s1 >> 18) & 1), c = operand(s2, (s2 >> 18) & 1);
        auto tex = [&](const char* kind) -> std::string {
            fi.texUsed |= 1u << texNum;
            const uint32_t dim = (texDims >> (2 * texNum)) & 3;
            std::string coord = bx2 ? "(" + a + "*2.0-1.0)" : a;
            std::string r;
            const std::string smp = "tex" + std::to_string(texNum), sc = "u_tsc[" + std::to_string(texNum) + "]";
            if (dim == 1) {   // cube
                if (!std::strcmp(kind, "txl")) r = "textureLod(" + smp + ", (" + coord + ").xyz, (" + coord + ").w)";
                else r = "texture(" + smp + ", (" + coord + ").xyz)";
            } else if (dim == 2) r = "texture(" + smp + ", (" + coord + ").xyz)";
            else if (dim == 3) {   // depth texture with a compare function: 1 where the test passes (2x2 taps)
                const bool proj = !std::strcmp(kind, "txp");
                r = "vec4(shcmp(" + smp + ", " + (proj ? "(" + coord + ").xyz / (" + coord + ").w" : "(" + coord + ").xyz") + ", " + sc + ".z))";
            } else {
                std::string uv = "(" + coord + ").xy";
                if (!std::strcmp(kind, "txp")) uv = "((" + coord + ").xy / (" + coord + ").w)";
                uv = "(" + uv + " * " + sc + ".xy)";
                if (!std::strcmp(kind, "txl")) r = "textureLod(" + smp + ", " + uv + ", (" + coord + ").w)";
                else if (!std::strcmp(kind, "txb")) r = "texture(" + smp + ", " + uv + ", (" + coord + ").w)";
                else if (!std::strcmp(kind, "txd")) r = "textureGrad(" + smp + ", " + uv + ", (" + bb + ").xy, (" + c + ").xy)";
                else r = "texture(" + smp + ", " + uv + ")";
            }
            return r;
        };
        std::string e;
        bool kill = false;
        switch (opcode) {
            case 0x00: break;
            case 0x01: e = a; break;
            case 0x02: e = a + "*" + bb; break;
            case 0x03: e = a + "+" + bb; break;
            case 0x04: e = a + "*" + bb + "+" + c; break;
            case 0x05: e = "vec4(dot((" + a + ").xyz,(" + bb + ").xyz))"; break;
            case 0x06: e = "vec4(dot(" + a + "," + bb + "))"; break;
            case 0x07: e = "vec4(1.0,(" + a + ").y*(" + bb + ").y,(" + a + ").z,(" + bb + ").w)"; break;
            case 0x08: e = "min(" + a + "," + bb + ")"; break;
            case 0x09: e = "max(" + a + "," + bb + ")"; break;
            case 0x0A: e = "vec4(lessThan(" + a + "," + bb + "))"; break;
            case 0x0B: e = "vec4(greaterThanEqual(" + a + "," + bb + "))"; break;
            case 0x0C: e = "vec4(lessThanEqual(" + a + "," + bb + "))"; break;
            case 0x0D: e = "vec4(greaterThan(" + a + "," + bb + "))"; break;
            case 0x0E: e = "vec4(notEqual(" + a + "," + bb + "))"; break;
            case 0x0F: e = "vec4(equal(" + a + "," + bb + "))"; break;
            case 0x10: e = "fract(" + a + ")"; break;
            case 0x11: e = "floor(" + a + ")"; break;
            case 0x12: kill = true; break;
            case 0x13: e = "vec4(uintBitsToFloat(packSnorm4x8(" + a + ")))"; break;
            case 0x14: e = "unpackSnorm4x8(floatBitsToUint((" + a + ").x))"; break;
            case 0x15: e = "dFdx(" + a + ")"; break;
            case 0x16: e = "dFdy(" + a + ")"; break;
            case 0x17: e = tex("tex"); break;
            case 0x18: e = tex("txp"); break;
            case 0x19: e = tex("txd"); break;
            case 0x1A: e = "vec4(1.0/(" + a + ").x)"; break;
            case 0x1B: e = "vec4(inversesqrt(abs((" + a + ").x)))"; break;
            case 0x1C: e = "vec4(exp2((" + a + ").x))"; break;
            case 0x1D: e = "vec4(log2((" + a + ").x))"; break;
            case 0x1E: e = "flit(" + a + ")"; break;
            case 0x1F: e = "mix(" + c + "," + bb + "," + a + ")"; break;
            case 0x20: e = "vec4(1.0)"; break;
            case 0x21: e = "vec4(0.0)"; break;
            case 0x22: e = "vec4(cos((" + a + ").x))"; break;
            case 0x23: e = "vec4(sin((" + a + ").x))"; break;
            case 0x24: e = "vec4(uintBitsToFloat(packHalf2x16((" + a + ").xy)))"; break;
            case 0x25: e = "unpackHalf2x16(floatBitsToUint((" + a + ").x)).xyxy"; break;
            case 0x26: e = "vec4(pow((" + a + ").x,(" + bb + ").x))"; break;
            case 0x27: e = "vec4(uintBitsToFloat(packUnorm4x8(" + a + ")))"; break;
            case 0x28: e = "unpackUnorm4x8(floatBitsToUint((" + a + ").x))"; break;
            case 0x29: e = "vec4(uintBitsToFloat(packSnorm2x16((" + a + ").xy)))"; break;
            case 0x2A: e = "unpackSnorm2x16(floatBitsToUint((" + a + ").x)).xyxy"; break;
            case 0x2C: e = "vec4(uintBitsToFloat(packUnorm4x8(sqrt(" + a + "))))"; break;
            case 0x2D: e = "pow(unpackUnorm4x8(floatBitsToUint((" + a + ").x)), vec4(2.0))"; break;
            case 0x2E: e = "vec4(dot((" + a + ").xy,(" + bb + ").xy)+(" + c + ").x)"; break;
            case 0x2F: e = tex("txl"); break;
            case 0x31: e = tex("txb"); break;
            case 0x36: e = "frefl(" + a + "," + bb + ")"; break;
            case 0x38: e = "vec4(dot((" + a + ").xy,(" + bb + ").xy))"; break;
            case 0x39: e = "vec4(normalize((" + a + ").xyz), 1.0)"; break;
            case 0x3A: e = a + "/(" + bb + ").x"; break;
            case 0x3B: e = a + "*inversesqrt(abs((" + bb + ").x))"; break;
            case 0x3D: case 0x3E: break;   // FENCT / FENCB
            case 0x42: {   // IFE: if (condition) ... else ... end
                const uint32_t elseOff = s1 & 0x7fffffff, endOff = s2 & 0x7fffffff;
                const uint32_t cs[4] = {(s0 >> 21) & 3, (s0 >> 23) & 3, (s0 >> 25) & 3, (s0 >> 27) & 3};
                const bool lt = (s0 >> 18) & 1, eq = (s0 >> 19) & 1, gt = (s0 >> 20) & 1;
                const uint32_t ccr = (s0 >> 31) & 1;
                std::snprintf(t, sizeof t, "  if (fcond(CC%u.%c%c%c%c, %d, %d, %d)) {\n", ccr, "xyzw"[cs[0]], "xyzw"[cs[1]], "xyzw"[cs[2]], "xyzw"[cs[3]], lt, eq, gt);
                b += t;
                if (endOff != elseOff) elseAt.push_back(elseOff);
                endAt.push_back(endOff);
                o += 16;
                if (hasConst) o += 16;
                if (dst & 1) n = 99999;
                continue;
            }
            default:
                fi.unsupported = true;
                std::snprintf(t, sizeof t, "  // unsupported opcode 0x%x\n", opcode);
                b += t;
                break;
        }
        // execution condition
        const bool lt = (s0 >> 18) & 1, eq = (s0 >> 19) & 1, gt = (s0 >> 20) & 1;
        const uint32_t cs[4] = {(s0 >> 21) & 3, (s0 >> 23) & 3, (s0 >> 25) & 3, (s0 >> 27) & 3};
        const uint32_t ccr = (s0 >> 31) & 1, ccw = (s0 >> 30) & 1;
        const bool always = lt && eq && gt;
        auto condOf = [&](int k) -> std::string {
            std::snprintf(t, sizeof t, "fcond1(CC%u.%c, %d, %d, %d)", ccr, "xyzw"[cs[k]], lt, eq, gt);
            return t;
        };
        if (kill) {
            if (always) b += "  discard;\n";
            else b += "  if (" + condOf(0) + " || " + condOf(1) + " || " + condOf(2) + " || " + condOf(3) + ") discard;\n";
        }
        if (!e.empty()) {
            const uint32_t scale = (s1 >> 28) & 7;
            static const char* scl[] = {"", "*2.0", "*4.0", "*8.0", "", "*0.5", "*0.25", "*0.125"};
            std::string v = "(" + e + ")" + scl[scale];
            if (sat) v = "clamp(" + v + ",0.0,1.0)";
            else if (prec == 2) v = "clamp(" + v + ",-2.0,2.0)";   // fx12
            else if (prec == 1) v = "clamp(" + v + ",-65504.0,65504.0)";
            b += "  t = " + v + ";\n";
            if (setCond) {
                for (int k = 0; k < 4; ++k)
                    if (mask[k]) { std::snprintf(t, sizeof t, "  CC%u.%c = t.%c;\n", ccw, "xyzw"[k], "xyzw"[k]); b += t; }
            }
            if (!noDest) {
                std::snprintf(t, sizeof t, "%s[%u]", fp16 ? "H" : "R", destReg);
                const std::string tg = t;
                if (destReg == 0) (fp16 ? fi.writesH0 : fi.writesR0) = true;
                for (int k = 0; k < 4; ++k) {
                    if (!mask[k]) continue;
                    const char comp = "xyzw"[k];
                    if (always) std::snprintf(t, sizeof t, "  %s.%c = t.%c;\n", tg.c_str(), comp, comp);
                    else std::snprintf(t, sizeof t, "  if (%s) %s.%c = t.%c;\n", condOf(k).c_str(), tg.c_str(), comp, comp);
                    b += t;
                }
            }
        }
        o += 16;
        if (hasConst) o += 16;
        if (dst & 1) break;
    }
    while (!elseAt.empty()) { b += "  } else {\n"; elseAt.pop_back(); }
    while (!endAt.empty()) { b += "  }\n"; endAt.pop_back(); }
    fi.size = o;
    static const bool shDbg = std::getenv("SHDBG") != nullptr;   // debug: shadow-map results as the output colour
    std::string h = std::string("#version 450\n") + (shDbg ? "#define SHDBG\nfloat g_shS = -1.0; vec2 g_shRD = vec2(0.0);\n" : "") + "#define SHBIAS " + std::string(std::getenv("SHBIAS") ? std::getenv("SHBIAS") : "0.002") + "\n" +
                    "in vec4 v_col0, v_col1, v_bfc0, v_bfc1, v_fog;\nin vec4 v_tc[10];\n"
                    "uniform vec4 u_tsc[16];\nuniform vec4 u_alpha;\nuniform vec4 u_wpos;\n" + std::string("uniform vec4 fc[") + std::to_string(std::max(1u, fi.nConst)) + "];\n" +
                    "layout(location = 0) out vec4 o_c0;\nlayout(location = 1) out vec4 o_c1;\n"
                    "layout(location = 2) out vec4 o_c2;\nlayout(location = 3) out vec4 o_c3;\n";
    for (int k = 0; k < 16; ++k) {
        if (!(fi.texUsed & (1u << k))) continue;
        const uint32_t dim = (texDims >> (2 * k)) & 3;
        std::snprintf(t, sizeof t, "layout(binding = %d) uniform %s tex%d;\n", k, dim == 1 ? "samplerCube" : dim == 2 ? "sampler3D" : "sampler2D", k);
        h += t;
    }
    h += "bool fcond1(float c, int lt, int eq, int gt) { return (lt != 0 && c < 0.0) || (eq != 0 && c == 0.0) || (gt != 0 && c > 0.0); }\n"
         "bool fcond(vec4 c, int lt, int eq, int gt) { return fcond1(c.x, lt, eq, gt) || fcond1(c.y, lt, eq, gt) || fcond1(c.z, lt, eq, gt) || fcond1(c.w, lt, eq, gt); }\n"
         // depth compare: stored depth OP reference (for GREATER the result is 1 where the surface is NOT behind an
         // occluder; the game shades with it as "lit"). Checked against the car selection screen, where the cars'
         // shadows are dark only this way. SHBIAS (default 0.002) moves the reference towards the light: no acne.
         "float shcmp(sampler2D t, vec3 p, float f) {\n"
         "  vec4 d = textureGather(t, p.xy, 0); vec4 r = vec4(p.z - SHBIAS); vec4 c;\n"
         "  if (f < 1.5) c = vec4(lessThan(d, r)); else if (f < 2.5) c = vec4(equal(d, r)); else if (f < 3.5) c = vec4(lessThanEqual(d, r));\n"
         "  else if (f < 4.5) c = vec4(greaterThan(d, r)); else if (f < 5.5) c = vec4(notEqual(d, r)); else c = vec4(greaterThanEqual(d, r));\n"
         // bilinear percentage-closer filtering like the hardware: the four results weighted by the position inside
         // the texel (an even average made the shadow edges blocky). textureGather order: (0,1) (1,1) (1,0) (0,0)
         "  vec2 fr = fract(p.xy * vec2(textureSize(t, 0)) - 0.5);\n"
         "  float s = mix(mix(c.w, c.z, fr.x), mix(c.x, c.y, fr.x), fr.y);\n"
         "#ifdef SHDBG\n  g_shS = s; g_shRD = vec2(p.z, d.w);\n#endif\n"
         "  return s;\n}\n"
         "vec4 flit(vec4 s) { float d = max(s.x, 0.0); float sp = s.x > 0.0 ? exp2(s.w * log2(max(s.y, 1e-30))) : 0.0; return vec4(1.0, d, sp, 1.0); }\n"
         "vec4 frefl(vec4 n, vec4 e) { return vec4(2.0 * dot(n.xyz, e.xyz) / dot(n.xyz, n.xyz) * n.xyz - e.xyz, 0.0); }\n"
         "vec4 col0() { return gl_FrontFacing ? v_col0 : v_col0; }\n"
         "vec4 col1() { return v_col1; }\n"
         "void main() {\n"
         "  vec4 R[64]; vec4 H[64]; for (int i = 0; i < 64; ++i) { R[i] = vec4(0.0); H[i] = vec4(0.0); }\n"
         "  vec4 t = vec4(0.0), CC0 = vec4(0.0), CC1 = vec4(0.0);\n"
         "  vec4 wpos = vec4(gl_FragCoord.x * u_wpos.x, gl_FragCoord.y * u_wpos.y, gl_FragCoord.z, 1.0 / gl_FragCoord.w);\n"
         "  vec4 ssa = vec4(gl_FrontFacing ? 1.0 : -1.0);\n";
    const bool out32 = (control & 0x40) != 0;
    const bool useR = fi.writesR0 && (!fi.writesH0 || out32);
    std::string tail;
    if (useR) tail = "  o_c0 = R[0]; o_c1 = R[2]; o_c2 = R[3]; o_c3 = R[4];\n";
    else tail = "  o_c0 = H[0]; o_c1 = H[4]; o_c2 = H[6]; o_c3 = H[8];\n";
    if (control & 0xe) tail += "  gl_FragDepth = R[1].z;\n";
    tail += "  if (u_alpha.x > 0.5) {\n"
            "    int f = int(u_alpha.y); float a = o_c0.a, r = u_alpha.z; bool pass = true;\n"
            "    if (f == 0) pass = false; else if (f == 1) pass = a < r; else if (f == 2) pass = a == r; else if (f == 3) pass = a <= r;\n"
            "    else if (f == 4) pass = a > r; else if (f == 5) pass = a != r; else if (f == 6) pass = a >= r;\n"
            "    if (!pass) discard;\n  }\n";
    if (std::getenv("RSXRED")) tail += "  o_c0 = v_col0;\n";
    if (shDbg) tail += std::getenv("SHDBG")[0] == '2' ? "  if (g_shS >= 0.0) o_c0 = vec4(g_shRD, g_shS, 1.0);\n" : "  if (g_shS >= 0.0) o_c0 = vec4(g_shS, 1.0 - g_shS, 0.0, 1.0);\n";   // red = shadow 1, green = 0
    fi.glsl = h + b + tail + "}\n";
    return fi;
}

// ------------------------------------------------------------------ programs
struct Program { uint64_t key = 0; GLuint prog = 0; GLint uVs, uVo, uSurf, uTsc, uAlpha, uWpos, uFc; uint32_t nConst = 0; uint32_t vpInputs = 0; uint32_t texUsed = 0; bool bad = false; };
static std::unordered_map<uint64_t, Program> programs;

static GLuint compile(GLenum type, const std::string& src, const char* what) {
    const GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]; GLsizei n = 0;
        glGetShaderInfoLog(s, sizeof log, &n, log);
        RLOG("[rsx] %s shader compile error:\n%s\n--- source ---\n%s\n", what, log, src.c_str());
    }
    return s;
}

static std::vector<float> g_fpConsts;   // constants of the fragment program of the current draw (filled by get_program)
static uint64_t vpGen = 1;              // bumped whenever vertex program words are uploaded
static Program* get_program(uint32_t vpStart, uint32_t fpEa, uint32_t texDims, uint32_t control) {
    // vertex program words up to the end bit
    uint32_t n = 0;
    while (vpStart + n < 512) { ++n; if (vpCode[(vpStart + n - 1) * 4 + 3] & 1) break; }
    static uint64_t vpKeyGen = 0, vpKey = 0; static uint32_t vpKeyStart = ~0u;
    if (vpKeyGen != vpGen || vpKeyStart != vpStart) { vpKey = fnv(vpCode + vpStart * 4, n * 16); vpKeyGen = vpGen; vpKeyStart = vpStart; }
    uint64_t key = vpKey;
    g_fpConsts.clear();
    key = fp_walk(fpEa, key, &g_fpConsts);
    key ^= (uint64_t(texDims) << 1) ^ (uint64_t(control & 0x4e) << 40) ^ uint64_t(vpStart) * 0x9E3779B97F4A7C15ull;
    auto it = programs.find(key);
    if (it != programs.end()) return &it->second;
    Program& p = programs[key];
    p.key = key;
    ++g_progBuilds;
    const VpInfo vi = translate_vp(vpCode, vpStart);
    const FpInfo fi = translate_fp(fpEa, texDims, control);
    p.vpInputs = vi.inputs;
    p.texUsed = fi.texUsed;
    p.nConst = fi.nConst;
    if (dumpShaders) {
        static int nShader = 0;
        char fn[128];
        std::snprintf(fn, sizeof fn, "rsxdump/prog_%04d_%016llx.glsl", nShader++, (unsigned long long)key);
        if (FILE* f = std::fopen(fn, "wb")) { std::fprintf(f, "// VP start %u len %d\n%s\n// FP %x size %u\n%s", vpStart, vi.len, vi.glsl.c_str(), fpEa, fi.size, fi.glsl.c_str()); std::fclose(f); }
    }
    if (vi.unsupported || fi.unsupported) RLOG("[rsx] program %016llx uses unsupported instructions (vp %d fp %d)\n", (unsigned long long)key, vi.unsupported, fi.unsupported);
    // Linked programs are kept on disk (glcache/): compiling one takes tens of milliseconds, a visible hitch the
    // first time an effect appears. The file name carries the hash of the generated GLSL, so a change in the
    // translator makes new files; a driver that rejects a binary just compiles again. RSXNOGLCACHE=1 turns it off.
    static const bool binCache = std::getenv("RSXNOGLCACHE") == nullptr;
    static bool dirMade = false;
    if (binCache && !dirMade) { dirMade = true; SDL_CreateDirectory("glcache"); }
    char cacheName[96];
    std::snprintf(cacheName, sizeof cacheName, "glcache/%016llx.bin", (unsigned long long)(fnv(vi.glsl.data(), vi.glsl.size(), fnv(fi.glsl.data(), fi.glsl.size()))));
    p.prog = glCreateProgram();
    GLint ok = 0;
    GLuint vs = 0, fs = 0;
    if (binCache)
        if (FILE* f = std::fopen(cacheName, "rb")) {
            std::vector<uint8_t> bin;
            uint8_t buf[65536]; size_t k;
            while ((k = std::fread(buf, 1, sizeof buf, f)) > 0) bin.insert(bin.end(), buf, buf + k);
            std::fclose(f);
            if (bin.size() > 4) {
                uint32_t fmt; std::memcpy(&fmt, bin.data(), 4);
                glProgramBinary(p.prog, fmt, bin.data() + 4, GLsizei(bin.size() - 4));
                glGetProgramiv(p.prog, GL_LINK_STATUS, &ok);
                while (glGetError()) {}
            }
        }
    if (!ok) {
        vs = compile(GL_VERTEX_SHADER, vi.glsl, "vertex"); fs = compile(GL_FRAGMENT_SHADER, fi.glsl, "fragment");
        glAttachShader(p.prog, vs);
        glAttachShader(p.prog, fs);
        glLinkProgram(p.prog);
        glGetProgramiv(p.prog, GL_LINK_STATUS, &ok);
        if (ok && binCache) {
            GLint len = 0;
            glGetProgramiv(p.prog, 0x8741 /* GL_PROGRAM_BINARY_LENGTH */, &len);
            if (len > 0) {
                std::vector<uint8_t> bin(size_t(len) + 4);
                GLsizei got = 0; GLenum fmt = 0;
                glGetProgramBinary(p.prog, len, &got, &fmt, bin.data() + 4);
                const uint32_t f32v = fmt; std::memcpy(bin.data(), &f32v, 4);
                if (got > 0) if (FILE* f = std::fopen(cacheName, "wb")) { std::fwrite(bin.data(), 1, size_t(got) + 4, f); std::fclose(f); }
            }
        }
    }
    if (!ok) {
        char log[4096]; GLsizei ln = 0;
        glGetProgramInfoLog(p.prog, sizeof log, &ln, log);
        RLOG("[rsx] link error: %s\n", log);
        p.bad = true;
    }
    glDeleteShader(vs); glDeleteShader(fs);
    p.uVs = glGetUniformLocation(p.prog, "u_vscale");
    p.uVo = glGetUniformLocation(p.prog, "u_voffset");
    p.uSurf = glGetUniformLocation(p.prog, "u_surf");
    p.uTsc = glGetUniformLocation(p.prog, "u_tsc");
    p.uAlpha = glGetUniformLocation(p.prog, "u_alpha");
    p.uWpos = glGetUniformLocation(p.prog, "u_wpos");
    p.uFc = glGetUniformLocation(p.prog, "fc");
    static int nProg = 0;
    if (++nProg % 50 == 0) RLOG("[rsx] %d programs compiled\n", nProg);
    return &p;
}

// ------------------------------------------------------------------ surfaces (render targets)
struct Surface { GLuint tex = 0; uint32_t w = 0, h = 0, fmt = 0; bool depth = false; uint64_t lastUse = 0; };
static std::map<uint64_t, Surface> surfaces;   // key: location<<32 | offset (+ depth flag)
static std::map<uint64_t, GLuint> fbos;

static void surface_gl_format(uint32_t fmt, GLenum& ifmt, GLenum& f, GLenum& ty) {
    switch (fmt) {
        case 3: ifmt = GL_RGB565; f = GL_RGB; ty = GL_UNSIGNED_SHORT_5_6_5; break;
        case 9: ifmt = GL_R8; f = GL_RED; ty = GL_UNSIGNED_BYTE; break;
        case 0xa: ifmt = GL_RG8; f = GL_RG; ty = GL_UNSIGNED_BYTE; break;
        case 0xb: ifmt = GL_RGBA16F; f = GL_RGBA; ty = GL_HALF_FLOAT; break;
        case 0xc: ifmt = GL_RGBA32F; f = GL_RGBA; ty = GL_FLOAT; break;
        case 0xd: ifmt = GL_R32F; f = GL_RED; ty = GL_FLOAT; break;
        default: ifmt = GL_RGBA8; f = GL_RGBA; ty = GL_UNSIGNED_BYTE; break;
    }
}

// Bumped whenever texture bindings made by bind_texture() may be stale: a surface (render target) appears or
// changes, a texture is created, re-uploaded or deleted, or other code binds textures (menu, cube copies).
static uint64_t g_texGen = 1;
static Surface& get_surface(uint32_t loc, uint32_t off, uint32_t w, uint32_t h, uint32_t fmt, bool depth) {
    const uint64_t key = (uint64_t(loc) << 33) | (uint64_t(depth) << 32) | off;
    const bool fresh = surfaces.find(key) == surfaces.end();
    Surface& s = surfaces[key];
    if (fresh) ++g_texGen;
    if (!s.tex || s.w != w || s.h != h || s.fmt != fmt) {
        // The game reuses an address with different sizes within a frame (the depth buffer of the 256x256 cube
        // faces and of the 2560x720 scene share one): the texture of each size is kept and comes back, instead of
        // being destroyed and created again several times per frame.
        struct Parked { uint64_t key; uint32_t w, h, fmt; GLuint tex; };
        static std::vector<Parked> parked;
        if (s.tex) parked.push_back({key, s.w, s.h, s.fmt, s.tex});
        s.tex = 0;
        for (size_t k = 0; k < parked.size(); ++k)
            if (parked[k].key == key && parked[k].w == w && parked[k].h == h && parked[k].fmt == fmt) { s.tex = parked[k].tex; parked.erase(parked.begin() + k); break; }
        if (s.tex) { s.w = w; s.h = h; s.fmt = fmt; s.depth = depth; s.lastUse = frameNo; return s; }
        glGenTextures(1, &s.tex);
        ++g_texGen;   // bound on the active unit below
        glBindTexture(GL_TEXTURE_2D, s.tex);
        if (depth) {
            if (fmt == 1) glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
            else glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        } else {
            GLenum ifmt, f, ty;
            surface_gl_format(fmt, ifmt, f, ty);
            glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, f, ty, nullptr);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        s.w = w; s.h = h; s.fmt = fmt; s.depth = depth;
        RLOG("[rsx] surface %s at %s 0x%x: %ux%u format %u\n", depth ? "depth" : "color", loc ? "main" : "local", off, w, h, fmt);
    }
    s.lastUse = frameNo;
    return s;
}

struct Target { GLuint fbo = 0; uint32_t w = 0, h = 0, aaX = 1, aaY = 1, logW = 0, logH = 0; int nColor = 0; };
static Target cur;

static void aa_factors(uint32_t aa, uint32_t& x, uint32_t& y) {
    switch (aa) { case 3: x = 2; y = 1; break; case 4: case 5: x = 2; y = 2; break; default: x = 1; y = 1; break; }
}

static bool bind_target() {
    const uint32_t fmtReg = R[0x208 >> 2];
    const uint32_t cfmt = fmtReg & 0x1f, zfmt = (fmtReg >> 5) & 7, aa = (fmtReg >> 12) & 0xf;
    const uint32_t clipW = R[0x200 >> 2] >> 16, clipH = R[0x204 >> 2] >> 16;
    const uint32_t clipX = R[0x200 >> 2] & 0xffff, clipY = R[0x204 >> 2] & 0xffff;
    uint32_t ax, ay;
    aa_factors(aa, ax, ay);
    if (clipX + clipW == 1280 && clipY + clipH == 720) { const uint32_t sc = uint32_t(settings().scale); ax *= sc; ay *= sc; }   // internal resolution: the full-screen surfaces only
    const uint32_t w = (clipX + clipW) * ax, h = (clipY + clipH) * ay;
    if (!w || !h) return false;
    const uint32_t ct = R[0x220 >> 2];
    static const uint32_t offReg[4] = {0x210, 0x218, 0x288, 0x28c}, dmaReg[4] = {0x194, 0x18c, 0x1b4, 0x1b8};
    int list[4], nList = 0;
    switch (ct & 0x1f) {
        case 0: break;
        case 1: list[nList++] = 0; break;
        case 2: list[nList++] = 1; break;
        case 0x13: list[nList++] = 0; list[nList++] = 1; break;
        case 0x17: list[nList++] = 0; list[nList++] = 1; list[nList++] = 2; break;
        case 0x1f: for (int k = 0; k < 4; ++k) list[nList++] = k; break;
        default: list[nList++] = 0; break;
    }
    GLuint texs[5] = {};
    uint64_t key = (uint64_t(w) << 48) ^ (uint64_t(h) << 32) ^ fmtReg;
    for (int k = 0; k < nList; ++k) {
        const int i = list[k];
        const uint32_t loc = R[dmaReg[i] >> 2] & 1;
        const uint32_t off = R[offReg[i] >> 2];
        texs[k] = get_surface(loc, off, w, h, cfmt, false).tex;
        key = fnv(&off, 4, key ^ (i + 1));
    }
    const bool hasZ = zfmt != 0;
    if (hasZ) {
        const uint32_t loc = R[0x198 >> 2] & 1, off = R[0x214 >> 2];
        texs[4] = get_surface(loc, off, w, h, zfmt, true).tex;
        key = fnv(&off, 4, key ^ 0x77);
    }
    key = fnv(texs, sizeof texs, key);
    GLuint& fbo = fbos[key];
    if (!fbo) {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum bufs[4];
        for (int k = 0; k < nList; ++k) { glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + k, GL_TEXTURE_2D, texs[k], 0); bufs[k] = GL_COLOR_ATTACHMENT0 + k; }
        if (hasZ) glFramebufferTexture2D(GL_FRAMEBUFFER, zfmt == 1 ? GL_DEPTH_ATTACHMENT : GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texs[4], 0);
        glDrawBuffers(nList, nList ? bufs : nullptr);
        if (!nList) { GLenum none = GL_NONE; glDrawBuffers(1, &none); }
        const GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) RLOG("[rsx] framebuffer incomplete 0x%x (colors %d, z %u, %ux%u)\n", st, nList, zfmt, w, h);
    } else glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    cur.fbo = fbo; cur.w = w; cur.h = h; cur.aaX = ax; cur.aaY = ay; cur.logW = clipX + clipW; cur.logH = clipY + clipH; cur.nColor = nList;
    glViewport(0, 0, w, h);
    return true;
}

static void apply_scissor() {
    // RSX scissor and viewport rectangle both clip (logical pixels; scaled by the AA factors)
    uint32_t sx = R[0x8c0 >> 2] & 0xffff, sw = R[0x8c0 >> 2] >> 16, sy = R[0x8c4 >> 2] & 0xffff, sh = R[0x8c4 >> 2] >> 16;
    const uint32_t vx = R[0xa00 >> 2] & 0xffff, vw = R[0xa00 >> 2] >> 16, vy = R[0xa04 >> 2] & 0xffff, vh = R[0xa04 >> 2] >> 16;
    if (sw == 0 && sh == 0) { sw = 4096; sh = 4096; }
    const int x0 = std::max(sx, vx), y0 = std::max(sy, vy);
    const int x1 = std::min(sx + sw, vx + vw), y1 = std::min(sy + sh, vy + vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0 * cur.aaX, y0 * cur.aaY, std::max(0, x1 - x0) * cur.aaX, std::max(0, y1 - y0) * cur.aaY);
}

static bool stateValid = false;   // false after anything else touched the GL state apply_state() sets
static void clear_surface(uint32_t mask) {
    if (!bind_target()) return;
    stateValid = false;
    apply_scissor();
    GLbitfield bits = 0;
    {   // RSXSTEP=frame: the main surface starts that frame in magenta, so every draw's contribution shows
        static const long stepFrame = std::getenv("RSXSTEP") ? std::atol(std::getenv("RSXSTEP")) : -1;
        if (long(frameNo) == stepFrame && cur.w == 2560 && !(mask & 0xf0)) {
            for (int k = 0; k < 4; ++k) glColorMaski(k, 1, 1, 1, 1);
            glClearColor(1, 0, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    if (mask & 0xf0) {
        const uint32_t c = R[0x1d90 >> 2];
        glClearColor(((c >> 16) & 0xff) / 255.f, ((c >> 8) & 0xff) / 255.f, (c & 0xff) / 255.f, (c >> 24) / 255.f);
        for (int k = 0; k < 4; ++k) glColorMaski(k, (mask >> 5) & 1, (mask >> 6) & 1, (mask >> 7) & 1, (mask >> 4) & 1);
        bits |= GL_COLOR_BUFFER_BIT;
    }
    const uint32_t zv = R[0x1d8c >> 2];
    const uint32_t zfmt = (R[0x208 >> 2] >> 5) & 7;
    if (mask & 1) {
        glDepthMask(1);
        glClearDepth(zfmt == 1 ? (zv & 0xffff) / 65535.0 : (zv >> 8) / 16777215.0);
        bits |= GL_DEPTH_BUFFER_BIT;
    }
    if ((mask & 2) && zfmt == 2) { glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xff); glClearStencil(zv & 0xff); bits |= GL_STENCIL_BUFFER_BIT; }
    if (bits) glClear(bits);
}

// ------------------------------------------------------------------ textures
struct Tex { GLuint id = 0; uint64_t sig = 0; uint64_t lastUse = 0; GLenum target = GL_TEXTURE_2D; uint64_t regSig = 0, checked = ~0ull, sampler = ~0ull; };
static std::unordered_map<uint64_t, Tex> texCache;

static uint32_t swizzle_index(uint32_t x, uint32_t y, uint32_t lw, uint32_t lh) {   // Morton order, x in the low bit
    uint32_t r = 0, bit = 0;
    for (uint32_t k = 0; k < 16 && (k < lw || k < lh); ++k) {
        if (k < lw) r |= ((x >> k) & 1) << bit++;
        if (k < lh) r |= ((y >> k) & 1) << bit++;
    }
    return r;
}
static uint32_t ilog2(uint32_t v) { uint32_t r = 0; while ((1u << r) < v) ++r; return r; }

struct TexFmt { GLenum ifmt, fmt, type; uint32_t bpp; bool dxt; uint32_t blockBytes; int swap; };   // swap: 0 none, 2 u16, 4 u32
static bool tex_format(uint32_t f, TexFmt& o) {
    switch (f) {
        case 0x81: o = {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1, false, 0, 0}; return true;
        case 0x82: o = {GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 2, false, 0, 2}; return true;
        case 0x83: o = {GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, 2, false, 0, 2}; return true;
        case 0x84: o = {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2, false, 0, 2}; return true;
        case 0x85: case 0x9e: o = {GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, 4, false, 0, 0}; return true;
        case 0x86: o = {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, 0, true, 8, 0}; return true;
        case 0x87: o = {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, 0, true, 16, 0}; return true;
        case 0x88: o = {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, 0, true, 16, 0}; return true;
        case 0x8b: o = {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2, false, 0, 0}; return true;
        case 0x94: o = {GL_R16, GL_RED, GL_UNSIGNED_SHORT, 2, false, 0, 2}; return true;
        case 0x95: o = {GL_RG16, GL_RG, GL_UNSIGNED_SHORT, 4, false, 0, 2}; return true;
        case 0x9a: o = {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8, false, 0, 2}; return true;
        case 0x9b: o = {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16, false, 0, 4}; return true;
        case 0x9c: o = {GL_R32F, GL_RED, GL_FLOAT, 4, false, 0, 4}; return true;
        case 0x9f: o = {GL_RG16F, GL_RG, GL_HALF_FLOAT, 4, false, 0, 2}; return true;
        default: return false;
    }
}

static GLenum wrap_of(uint32_t w) {
    switch (w) { case 1: return GL_REPEAT; case 2: return GL_MIRRORED_REPEAT; case 3: return GL_CLAMP_TO_EDGE; case 4: return GL_CLAMP_TO_BORDER;
                 case 5: return GL_CLAMP_TO_EDGE; case 6: case 8: return GL_MIRROR_CLAMP_TO_EDGE; default: return GL_CLAMP_TO_EDGE; }
}

// uploads (or finds) texture unit i; returns dimension code for the shader key and the coordinate scale
static uint32_t texDimsOf(int i) {
    const uint32_t base = i < 8 ? 0x1a00 + 0x20 * i : 0x1b00 + 0x20 * (i - 8);
    const uint32_t fmtReg = R[(base + 4) >> 2];
    const uint32_t dim = (fmtReg >> 4) & 0xf;
    if (fmtReg & 4) return 1;
    if (dim == 3) return 2;
    const uint32_t f = (fmtReg >> 8) & 0x9f;   // DEPTH24_D8 / DEPTH24_D8_FLOAT / DEPTH16 / DEPTH16_FLOAT
    if (f >= 0x90 && f <= 0x93 && ((R[(base + 8) >> 2] >> 28) & 7) != 7 && ((R[(base + 8) >> 2] >> 28) & 7) != 0) return 3;   // shadow map: depth compare
    return 0;
}

// NV4097_SET_TEXTURE_BORDER_COLOR (A8R8G8B8): what CLAMP_TO_BORDER samples outside the texture, e.g. the white
// around a projected shadow that multiplies the road
static void apply_border_color(GLenum target, uint32_t base) {
    const uint32_t bc = R[(base + 0x1c) >> 2];
    const float c[4] = {((bc >> 16) & 0xff) / 255.f, ((bc >> 8) & 0xff) / 255.f, (bc & 0xff) / 255.f, (bc >> 24) / 255.f};
    glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, c);
}

static uint64_t g_memoHits = 0, g_memoMiss[4] = {};   // RSXPROF: texture-unit memo hits; misses: no memo, generation, age, registers
static bool g_bindMemoOk = false;   // set by bind_texture_impl when the binding is a plain cached texture
static void bind_texture_impl(int i, float* tsc);
// Per texture unit: the registers of its last binding. A draw that samples the same texture as the previous draw
// on that unit (most of them: >60000 draws per second) then costs nothing; the content check still runs every
// few frames because the memo expires.
static GLuint g_lastId = 0; static GLenum g_lastTarget = 0; static uint64_t g_lastSurf = 0;   // surface key | bit 63, 0 = plain texture   // what bind_texture_impl bound when g_bindMemoOk
// Bindings already worked out, keyed by the unit's registers (any unit): a draw that samples a texture seen before
// (most of the >60000 draws per second) only binds its GL texture, or nothing when the unit already holds it.
// Valid while no texture/surface changed (g_texGen) and for 4 frames (the content check), and only if the GL
// texture still carries the sampler state of these registers (another unit may have set other filters on it).
static void bind_texture(int i, float* tsc) {
    struct Fast { uint64_t gen, frame, surf; GLuint id; GLenum target; float tsc[4]; };
    static std::unordered_map<uint64_t, Fast> fast;
    static std::unordered_map<GLuint, uint64_t> paramsOf;   // GL texture -> registers its sampler state was set for
    static GLuint boundOn[16] = {};
    static uint64_t boundGen = 0;
    static const bool off = std::getenv("RSXNOTEXMEMO") != nullptr;
    if (boundGen != g_texGen) { std::memset(boundOn, 0, sizeof boundOn); boundGen = g_texGen; }
    const uint32_t base = i < 8 ? 0x1a00 + 0x20 * i : 0x1b00 + 0x20 * (i - 8);
    uint32_t regs[9];
    for (int k = 0; k < 8; ++k) regs[k] = R[(base >> 2) + k];
    regs[8] = R[(0x1840 + 4 * i) >> 2];
    const uint64_t h = fnv(regs, sizeof regs);
    if (!off) {
        auto it = fast.find(h);
        if (it != fast.end() && it->second.gen == g_texGen && frameNo - it->second.frame < 4) {
            const Fast& f = it->second;
            auto pit = paramsOf.find(f.id);
            bool ok = pit != paramsOf.end() && pit->second == h;
            if (ok && f.surf) { auto sit = surfaces.find(f.surf & ~(1ull << 63)); ok = sit != surfaces.end() && sit->second.tex == f.id; }
            if (ok) {
                if (boundOn[i] != f.id) { glActiveTexture(GL_TEXTURE0 + i); glBindTexture(f.target, f.id); boundOn[i] = f.id; }
                std::memcpy(tsc, f.tsc, sizeof f.tsc);
                ++g_memoHits;
                return;
            }
            ++g_memoMiss[3];
        } else ++g_memoMiss[it == fast.end() ? 0 : it->second.gen != g_texGen ? 1 : 2];
    }
    g_bindMemoOk = false; g_lastSurf = 0;
    bind_texture_impl(i, tsc);
    if (g_bindMemoOk && g_lastId) {
        Fast& f = fast[h];
        f.gen = g_texGen; f.frame = frameNo; f.surf = g_lastSurf; f.id = g_lastId; f.target = g_lastTarget; std::memcpy(f.tsc, tsc, sizeof f.tsc);
        paramsOf[g_lastId] = h;
        boundOn[i] = g_lastId;
        if (fast.size() > 20000) fast.clear();
    } else boundOn[i] = 0;   // unknown: bound by the slow path (cube, unsupported format...)
}
static void bind_texture_impl(int i, float* tsc) {
    const uint32_t base = i < 8 ? 0x1a00 + 0x20 * i : 0x1b00 + 0x20 * (i - 8);
    const uint32_t off = R[base >> 2], fmtReg = R[(base + 4) >> 2], addr = R[(base + 8) >> 2], ctl0 = R[(base + 0xc) >> 2];
    const uint32_t remap = R[(base + 0x10) >> 2], filt = R[(base + 0x14) >> 2], rect = R[(base + 0x18) >> 2];
    const uint32_t ctl3 = R[(0x1840 + 4 * i) >> 2];
    const uint32_t loc = (fmtReg & 3) == 2 ? 1 : 0;
    const bool cube = fmtReg & 4;
    const uint32_t f = (fmtReg >> 8) & 0xff, baseFmt = f & ~0x60u;
    const bool ln = f & 0x20, un = f & 0x40;
    const uint32_t mips = std::max<uint32_t>(1, fmtReg >> 16);
    const uint32_t w = std::max<uint32_t>(1, rect >> 16), h = std::max<uint32_t>(1, rect & 0xffff);
    const uint32_t pitch = ctl3 & 0xfffff;
    tsc[0] = un ? 1.f / w : 1.f; tsc[1] = un ? 1.f / h : 1.f; tsc[2] = float((addr >> 28) & 7); tsc[3] = 1.f;   // z: depth compare function (shcmp)
    glActiveTexture(GL_TEXTURE0 + i);
    if (std::getenv("SHADOWLOG") && texDimsOf(i) == 3) {   // debug: shadow-map lookups
        static std::set<uint64_t> seen;
        const bool hasDepth = surfaces.count((uint64_t(loc) << 33) | (uint64_t(1) << 32) | off), hasColor = surfaces.count((uint64_t(loc) << 33) | off);
        if (seen.size() < 40) RLOG("[shadow] border color register %08x\n", R[(base + 0x1c) >> 2]);
        if (seen.insert(uint64_t(off) << 8 | uint64_t(i) | uint64_t((addr >> 28) & 7) << 40 | uint64_t(R[0x1d60 >> 2]) << 44).second && seen.size() < 40)
            RLOG("[shadow] unit %d: depth texture at %s 0x%x %ux%u fmt %02x func %u wrap %x: depth surface %s, color surface %s\n", i, loc ? "main" : "local", off, w, h, f, (addr >> 28) & 7, addr & 0xfff, hasDepth ? "found" : "missing", hasColor ? "found" : "missing");
    }
    // render target?
    for (int z = 0; z < 2; ++z) {
        auto it = surfaces.find((uint64_t(loc) << 33) | (uint64_t(z) << 32) | off);
        if (it != surfaces.end() && !cube) {
            glBindTexture(GL_TEXTURE_2D, it->second.tex);
            g_bindMemoOk = true; g_lastId = it->second.tex; g_lastTarget = GL_TEXTURE_2D; g_lastSurf = it->first | (1ull << 63);
            const GLint sw[4] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, sw);
            const uint32_t mag = (filt >> 24) & 7;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag == 1 ? GL_NEAREST : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag == 1 ? GL_NEAREST : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_of(addr & 0xf));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_of((addr >> 8) & 0xf));
            apply_border_color(GL_TEXTURE_2D, base);
            return;
        }
    }
    if (cube) {   // cube map whose faces are render targets (the environment the cars and windows reflect)
        const uint32_t faceStride = (w * h * 4 + 127) & ~127u;
        const uint64_t key0 = (uint64_t(loc) << 33) | off;
        if (surfaces.find(key0) != surfaces.end()) {
            struct RenderedCube { GLuint id = 0; uint64_t built = ~0ull; uint32_t w = 0, h = 0; };
            static std::map<uint64_t, RenderedCube> rcubes;
            static GLuint copyFbo = 0;
            RenderedCube& rc = rcubes[key0];
            if (!rc.id || rc.w != w || rc.h != h) {
                ++g_texGen;
                if (rc.id) glDeleteTextures(1, &rc.id);
                glGenTextures(1, &rc.id);
                glBindTexture(GL_TEXTURE_CUBE_MAP, rc.id);
                for (int k = 0; k < 6; ++k) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + k, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                rc.w = w; rc.h = h; rc.built = ~0ull;
            }
            glBindTexture(GL_TEXTURE_CUBE_MAP, rc.id);
            if (rc.built != frameNo) {   // once per frame: the faces are drawn before the scene that uses them
                rc.built = frameNo;
                if (!copyFbo) glGenFramebuffers(1, &copyFbo);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, copyFbo);
                for (uint32_t k = 0; k < 6; ++k) {
                    auto it = surfaces.find(key0 + uint64_t(k) * faceStride);
                    if (it == surfaces.end()) continue;
                    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, it->second.tex, 0);
                    glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + k, 0, 0, 0, 0, 0, GLsizei(std::min(w, it->second.w)), GLsizei(std::min(h, it->second.h)));
                }
                glBindFramebuffer(GL_READ_FRAMEBUFFER, cur.fbo);
            }
            const GLint sw[4] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
            glTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_RGBA, sw);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            return;
        }
    }
    TexFmt tf;
    if (!tex_format(baseFmt, tf)) {
        static std::map<uint32_t, int> warned;
        if (!warned[baseFmt]++) RLOG("[rsx] texture format 0x%x unsupported (unit %d)\n", baseFmt, i);
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }
    const uint32_t ea = ea_of(off, loc);
    // size of the image data (all faces and levels)
    auto levelBytes = [&](uint32_t lw, uint32_t lh) -> uint32_t {
        if (tf.dxt) return std::max(1u, (lw + 3) / 4) * std::max(1u, (lh + 3) / 4) * tf.blockBytes;
        if (ln && pitch) return pitch * lh;
        return lw * lh * tf.bpp;
    };
    uint32_t faceBytes = 0;
    for (uint32_t l = 0, lw = w, lh = h; l < mips; ++l, lw = std::max(1u, lw / 2), lh = std::max(1u, lh / 2)) faceBytes += levelBytes(lw, lh);
    const uint32_t faces = cube ? 6 : 1;
    const uint32_t faceStride = cube ? (faceBytes + 127) & ~127u : faceBytes;
    const uint32_t total = faceStride * faces;
    if (uint64_t(ea) + total > 0x100000000ull) return;
    // signature: format registers + sampled content (the content is sampled once per frame and texture)
    const uint64_t key = (uint64_t(loc) << 32 | off) ^ (uint64_t(cube) << 63);
    Tex& tx = texCache[key];
    g_bindMemoOk = true;
    const uint64_t regSig = fnv(&fmtReg, 4) ^ (uint64_t(rect) << 20) ^ (uint64_t(pitch) << 3) ^ remap * 0x100000001b3ull;
    uint64_t sig = tx.sig;
    // The content is re-sampled every 4th frame only (and whenever the registers change): textures are rarely
    // rewritten in place (render targets are handled apart), and this sampling was a third of the RSX thread.
    if (!tx.id || tx.regSig != regSig || frameNo - tx.checked >= 4 || tx.checked > frameNo) {
        sig = regSig;
        const uint32_t step = std::max<uint32_t>(64, total / 512);
        for (uint32_t k = 0; k + 8 <= total; k += step) sig = fnv(C.mem + ea + k, 8, sig);
        sig = fnv(C.mem + ea + total - std::min<uint32_t>(total, 8), std::min<uint32_t>(total, 8), sig);
        tx.checked = frameNo; tx.regSig = regSig;
    }
    const GLenum target = cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    if (!tx.id || tx.sig != sig || tx.target != target) {
        if (tx.id && tx.target != target) { ++g_texGen; glDeleteTextures(1, &tx.id); tx.id = 0; }   // re-uploads keep the GL name: bindings stay valid
        if (!tx.id) glGenTextures(1, &tx.id);
        tx.target = target;
        tx.sig = sig;
        tx.sampler = ~0ull;
        ++g_texUploads; g_texUploadBytes += total;
        glBindTexture(target, tx.id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        std::vector<uint8_t> buf;
        for (uint32_t face = 0; face < faces; ++face) {
            uint32_t p = ea + face * faceStride;
            const GLenum tgt = cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : GL_TEXTURE_2D;
            for (uint32_t l = 0, lw = w, lh = h; l < mips; ++l, lw = std::max(1u, lw / 2), lh = std::max(1u, lh / 2)) {
                const uint32_t lb = levelBytes(lw, lh);
                if (tf.dxt) {
                    glCompressedTexImage2D(tgt, l, tf.ifmt, lw, lh, 0, lb, C.mem + p);
                } else {
                    const bool swz = !ln && (lw & (lw - 1)) == 0 && (lh & (lh - 1)) == 0;
                    const uint32_t lwl = ilog2(lw), lhl = ilog2(lh);
                    if (!swz && tf.swap == 0 && baseFmt != 0x9e) {   // rows as they are in memory: no copy at all
                        if (ln && pitch) glPixelStorei(GL_UNPACK_ROW_LENGTH, GLint(pitch / tf.bpp));
                        glTexImage2D(tgt, l, tf.ifmt, lw, lh, 0, tf.fmt, tf.type, C.mem + p);
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                        p += lb;
                        continue;
                    }
                    buf.resize(size_t(lw) * lh * tf.bpp);
                    if (swz && tf.bpp == 4 && tf.swap == 0) {   // swizzled 32-bit texels: x and y bits never overlap, one table each
                        static std::vector<uint32_t> xs, ys;
                        xs.resize(lw); ys.resize(lh);
                        for (uint32_t x = 0; x < lw; ++x) xs[x] = swizzle_index(x, 0, lwl, lhl) * 4;
                        for (uint32_t y = 0; y < lh; ++y) ys[y] = swizzle_index(0, y, lwl, lhl) * 4;
                        const uint8_t* src = C.mem + p;
                        uint32_t* d = reinterpret_cast<uint32_t*>(buf.data());
                        for (uint32_t y = 0; y < lh; ++y) {
                            const uint8_t* row = src + ys[y];
                            for (uint32_t x = 0; x < lw; ++x) std::memcpy(d++, row + xs[x], 4);
                        }
                    } else
                    for (uint32_t y = 0; y < lh; ++y)
                        for (uint32_t x = 0; x < lw; ++x) {
                            const uint32_t src = swz ? swizzle_index(x, y, lwl, lhl) * tf.bpp : (ln && pitch ? y * pitch + x * tf.bpp : (y * lw + x) * tf.bpp);
                            uint8_t* d = buf.data() + (size_t(y) * lw + x) * tf.bpp;
                            const uint8_t* s = C.mem + p + src;
                            if (tf.swap == 2) for (uint32_t k = 0; k < tf.bpp; k += 2) { d[k] = s[k + 1]; d[k + 1] = s[k]; }
                            else if (tf.swap == 4) for (uint32_t k = 0; k < tf.bpp; k += 4) { d[k] = s[k + 3]; d[k + 1] = s[k + 2]; d[k + 2] = s[k + 1]; d[k + 3] = s[k]; }
                            else std::memcpy(d, s, tf.bpp);
                        }
                    if (baseFmt == 0x9e) for (size_t k = 0; k < buf.size(); k += 4) buf[k] = 0xff;   // D8R8G8B8: alpha = 1 (byte 0 = A in memory)
                    glTexImage2D(tgt, l, tf.ifmt, lw, lh, 0, tf.fmt, tf.type, buf.data());
                }
                p += lb;
            }
        }
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, mips - 1);
    } else glBindTexture(target, tx.id);
    tx.lastUse = frameNo;
    g_lastId = tx.id; g_lastTarget = target;
    {   // the sampler state lives in the GL texture object: set it only when the registers changed
        const uint32_t sreg[7] = {addr, ctl0, filt, remap, R[(base + 0x1c) >> 2], mips, uint32_t(settings().aniso)};
        const uint64_t sk = fnv(sreg, sizeof sreg);
        if (tx.sampler == sk) return;
        tx.sampler = sk;
    }
    // remap: per output channel (A, R, G, B) a source channel and a mode (0 zero, 1 one, 2 source)
    {
        const GLint srcCh[4] = {GL_ALPHA, GL_RED, GL_GREEN, GL_BLUE};
        GLint sw[4];
        const int outOrder[4] = {1, 2, 3, 0};   // GL swizzle order R G B A <- RSX A=0 R=1 G=2 B=3
        for (int k = 0; k < 4; ++k) {
            const int rc = outOrder[k];
            const uint32_t sel = (remap >> (2 * rc)) & 3, mode = (remap >> (8 + 2 * rc)) & 3;
            GLint v = srcCh[sel];
            if (tf.fmt == GL_RED) v = GL_RED;
            else if (tf.fmt == GL_RG) v = (sel == 1 || sel == 3) ? GL_GREEN : GL_RED;
            if (mode == 0) v = GL_ZERO; else if (mode == 1) v = GL_ONE;
            sw[k] = v;
        }
        glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, sw);
    }
    const uint32_t minF = (filt >> 16) & 0xff, magF = (filt >> 24) & 7;
    static const GLenum minTab[] = {GL_NEAREST, GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR};
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mips > 1 ? minTab[minF & 7] : (minF == 1 || minF == 3 || minF == 5 ? GL_NEAREST : GL_LINEAR));
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, magF == 1 ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap_of(addr & 0xf));
    glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap_of((addr >> 8) & 0xf));
    glTexParameteri(target, GL_TEXTURE_WRAP_R, wrap_of((addr >> 16) & 0xf));
    apply_border_color(target, base);
    glTexParameterf(target, GL_TEXTURE_MIN_LOD, ((ctl0 >> 19) & 0xfff) / 256.f);
    glTexParameterf(target, GL_TEXTURE_MAX_LOD, ((ctl0 >> 7) & 0xfff) / 256.f);
    // the game's own anisotropy, raised to the option's (mipmapped textures only: the road and walls seen at a
    // grazing angle stay sharp in the distance instead of blurring)
    const uint32_t gameAniso = std::min<uint32_t>(4, (ctl0 >> 4) & 7);
    const uint32_t an = mips > 1 ? std::max<uint32_t>(gameAniso, uint32_t(std::clamp(settings().aniso, 0, 4))) : gameAniso;
    glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY, float(1u << an));
    if (std::getenv("ANISOLOG")) { static int n = 0; if (n++ < 12) RLOG("[aniso] texture %ux%u, %u levels, filter %08x, game %ux -> %ux%c", w, h, mips, filt, 1u << gameAniso, 1u << an, 10); }
}

// ------------------------------------------------------------------ vertex data and draws
static std::vector<float> vtx;
static std::vector<uint32_t> idx;

static void convert_attr(int a, uint32_t first, uint32_t count, float* out) {
    const uint32_t fmt = R[(0x1740 + 4 * a) >> 2], offReg = R[(0x1680 + 4 * a) >> 2];
    const uint32_t type = fmt & 0xf, size = std::min<uint32_t>((fmt >> 4) & 0xf, 4), stride = (fmt >> 8) & 0xff;
    const uint32_t base = ea_of(offReg & 0x7fffffff, offReg >> 31) + R[0x1738 >> 2];
    const uint8_t* p = C.mem + base + size_t(stride) * first;
    // one tight loop per type (this runs for every vertex of every draw); missing components are 0, 0, 0, 1
    const auto r16 = [](const uint8_t* q) { uint16_t v; std::memcpy(&v, q, 2); return uint16_t(__builtin_bswap16(v)); };
    const auto r32 = [](const uint8_t* q) { uint32_t v; std::memcpy(&v, q, 4); return __builtin_bswap32(v); };
#define RSX_CONV(EXPR)     for (uint32_t v = 0; v < count; ++v, p += stride, out += 4) {         out[0] = 0; out[1] = 0; out[2] = 0; out[3] = 1;         for (uint32_t k = 0; k < size; ++k) out[k] = (EXPR);     }
    switch (type) {
        case 1: RSX_CONV(std::max(-1.f, int16_t(r16(p + 2 * k)) / 32767.f)) break;
        case 2:
            if (size == 3) for (uint32_t v = 0; v < count; ++v, p += stride, out += 4) { const uint32_t x[4] = {r32(p), r32(p + 4), r32(p + 8), 0x3f800000u}; std::memcpy(out, x, 16); }
            else if (size == 4) for (uint32_t v = 0; v < count; ++v, p += stride, out += 4) { const uint32_t x[4] = {r32(p), r32(p + 4), r32(p + 8), r32(p + 12)}; std::memcpy(out, x, 16); }
            else if (size == 2) for (uint32_t v = 0; v < count; ++v, p += stride, out += 4) { const uint32_t x[4] = {r32(p), r32(p + 4), 0u, 0x3f800000u}; std::memcpy(out, x, 16); }
            else RSX_CONV(f32(r32(p + 4 * k)))
            break;
        case 3: RSX_CONV(half2f(r16(p + 2 * k))) break;
        case 4: RSX_CONV(p[k] * (1.f / 255.f)) break;
        case 5: RSX_CONV(float(int16_t(r16(p + 2 * k)))) break;
        case 6:
            for (uint32_t v = 0; v < count; ++v, p += stride, out += 4) {
                const uint32_t w = r32(p);
                out[0] = float(int32_t(w << 21) >> 21) / 1023.f;
                out[1] = float(int32_t(w << 10) >> 21) / 1023.f;
                out[2] = float(int32_t(w) >> 22) / 511.f;
                out[3] = 1;
            }
            break;
        case 7: RSX_CONV(float(p[k])) break;
        default: for (uint32_t v = 0; v < count; ++v, out += 4) { out[0] = 0; out[1] = 0; out[2] = 0; out[3] = 1; } break;
    }
#undef RSX_CONV
}

static GLenum gl_prim(uint32_t p) {
    switch (p) { case 1: return GL_POINTS; case 2: return GL_LINES; case 3: return GL_LINE_LOOP; case 4: return GL_LINE_STRIP; case 5: return GL_TRIANGLES;
                 case 6: case 9: return GL_TRIANGLE_STRIP; case 7: case 10: return GL_TRIANGLE_FAN; case 8: return GL_TRIANGLES; default: return GL_TRIANGLES; }
}

static void apply_state() {
    static const uint16_t regs[] = {0x310, 0x314, 0x318, 0x320, 0x31c, 0x324, 0xa74, 0xa6c, 0xa70, 0x328, 0x330, 0x334, 0x338, 0x33c, 0x340, 0x344, 0x32c,
                                    0x183c, 0x1830, 0x1834, 0xa68, 0xa78, 0xa7c};
    static uint32_t last[sizeof regs / sizeof regs[0]];
    uint32_t now[sizeof regs / sizeof regs[0]];
    for (size_t k = 0; k < sizeof regs / sizeof regs[0]; ++k) now[k] = R[regs[k] >> 2];
    if (stateValid && !std::memcmp(now, last, sizeof now)) return;
    std::memcpy(last, now, sizeof now);
    stateValid = true;
    const auto en = [](GLenum c, bool on) { on ? glEnable(c) : glDisable(c); };
    en(GL_BLEND, R[0x310 >> 2] & 1);
    const uint32_t sf = R[0x314 >> 2], df = R[0x318 >> 2], eq = R[0x320 >> 2], bc = R[0x31c >> 2];
    glBlendFuncSeparate(sf & 0xffff, df & 0xffff, sf >> 16, df >> 16);
    glBlendEquationSeparate((eq & 0xffff) ? (eq & 0xffff) : 0x8006, (eq >> 16) ? (eq >> 16) : 0x8006);
    glBlendColor(((bc >> 16) & 0xff) / 255.f, ((bc >> 8) & 0xff) / 255.f, (bc & 0xff) / 255.f, (bc >> 24) / 255.f);
    const uint32_t cm = R[0x324 >> 2];
    for (int k = 0; k < 4; ++k) glColorMaski(k, (cm >> 16) & 1, (cm >> 8) & 1, cm & 1, (cm >> 24) & 1);
    en(GL_DEPTH_TEST, R[0xa74 >> 2] & 1);
    glDepthFunc(R[0xa6c >> 2] ? R[0xa6c >> 2] : 0x201);
    glDepthMask(R[0xa70 >> 2] & 1);
    en(GL_STENCIL_TEST, R[0x328 >> 2] & 1);
    if (R[0x328 >> 2] & 1) {
        glStencilFuncSeparate(GL_FRONT_AND_BACK, R[0x330 >> 2] ? R[0x330 >> 2] : 0x207, R[0x334 >> 2], R[0x338 >> 2]);
        glStencilOpSeparate(GL_FRONT_AND_BACK, R[0x33c >> 2] ? R[0x33c >> 2] : 0x1e00, R[0x340 >> 2] ? R[0x340 >> 2] : 0x1e00, R[0x344 >> 2] ? R[0x344 >> 2] : 0x1e00);
        glStencilMaskSeparate(GL_FRONT_AND_BACK, R[0x32c >> 2]);
    }
    static const bool noCull = std::getenv("RSXNOCULL") != nullptr;
    en(GL_CULL_FACE, !noCull && (R[0x183c >> 2] & 1));
    glCullFace(R[0x1830 >> 2] ? R[0x1830 >> 2] : 0x405);
    // the surface is drawn upside down in GL (row 0 = top), which flips the winding
    const uint32_t ff = R[0x1834 >> 2] ? R[0x1834 >> 2] : GL_CCW;
    glFrontFace(ff == GL_CW ? GL_CCW : GL_CW);
    en(GL_POLYGON_OFFSET_FILL, R[0xa68 >> 2] & 1);
    glPolygonOffset(rf(0xa78), rf(0xa7c));
}

// Vertex streams in RSX local memory (texture coordinates, normals, colours of the level and the cars) do not
// change from frame to frame: they are converted to floats once and kept in a GL buffer. Edge geometry output
// (positions, in main memory) is new every frame and goes through the per-draw path. The content of a cached
// stream is sampled once per frame, so a level change that reuses the address is noticed.
struct Stream { GLuint vbo = 0; uint32_t count = 0; uint64_t sig = 0, checked = ~0ull; };
static std::map<uint64_t, Stream> streams;
static uint64_t stream_sig(const uint8_t* p, uint32_t stride, uint32_t count) {
    uint64_t h = fnv(&count, 4);
    const uint32_t step = std::max<uint32_t>(1, count / 24);
    for (uint32_t v = 0; v < count; v += step) h = fnv(p + size_t(v) * stride, std::min<uint32_t>(stride, 8), h);
    return fnv(p + size_t(count - 1) * stride, std::min<uint32_t>(stride, 8), h);
}
// GL buffer with attribute a converted for vertices 0..need-1 (4 floats each), or 0 when the stream is not cacheable
static GLuint cached_stream(int a, uint32_t need) {
    static const bool off = std::getenv("RSXNOSTREAMCACHE") != nullptr;
    const uint32_t fmt = R[(0x1740 + 4 * a) >> 2], offReg = R[(0x1680 + 4 * a) >> 2];
    const uint32_t stride = (fmt >> 8) & 0xff;
    if (off || (offReg >> 31) || stride == 0 || need == 0 || need > 2000000) return 0;
    const uint32_t base = ea_of(offReg & 0x7fffffff, 0) + R[0x1738 >> 2];
    if (uint64_t(base) + uint64_t(need + need / 4 + 64) * stride >= 0xfff00000ull) return 0;
    Stream& st = streams[uint64_t(base) << 16 | (fmt & 0xffff)];
    const uint8_t* p = C.mem + base;
    if (st.count && st.checked != frameNo) {
        st.checked = frameNo;
        if (stream_sig(p, stride, st.count) != st.sig) st.count = 0;   // the memory holds something else now
    }
    if (st.count < need) {
        static std::vector<float> tmp;
        const uint32_t n = need + need / 4 + 64;   // room to grow: other draws of the object use later vertices
        tmp.resize(size_t(n) * 4);
        convert_attr(a, 0, n, tmp.data());
        if (!st.vbo) glGenBuffers(1, &st.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, st.vbo);
        glBufferData(GL_ARRAY_BUFFER, tmp.size() * 4, tmp.data(), 0x88E4 /* GL_STATIC_DRAW */);
        st.count = n; st.sig = stream_sig(p, stride, n); st.checked = frameNo;
        ++g_streamBuilds; g_streamVerts += n;
    }
    return st.vbo;
}

// RSXPROF=1: time inside do_draw by stage (ms per second) and totals per second
static uint64_t pfT[6], pfDraws, pfVerts, pfIdx, pfLast;
static const char* const pfName[6] = {"target+program", "state+uniforms", "textures", "indices", "vertex conversion", "upload+draw"};
static void do_draw() {
    if (ranges.empty() || !glOk) return;
    struct SlowDraw {   // a draw over 6 ms: what it had to build
        uint64_t t0 = SDL_GetPerformanceCounter(), tu = g_texUploads, tb = g_texUploadBytes, sb = g_streamBuilds, sv = g_streamVerts, pb = g_progBuilds;
        ~SlowDraw() {
            const double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency();
            if (ms > 6) RLOG("[slowdraw] %.0f ms: %llu textures uploaded (%llu KB), %llu vertex streams built (%llu vertices), %llu programs built\n", ms, (unsigned long long)(g_texUploads - tu),
                             (unsigned long long)((g_texUploadBytes - tb) >> 10), (unsigned long long)(g_streamBuilds - sb), (unsigned long long)(g_streamVerts - sv), (unsigned long long)(g_progBuilds - pb));
        }
    } slowDraw;
    static const bool prof = std::getenv("RSXPROF") != nullptr;
    uint64_t pt = prof ? SDL_GetPerformanceCounter() : 0;
    int stage = 0;
    auto mark = [&] { if (prof) { const uint64_t n = SDL_GetPerformanceCounter(); pfT[stage++] += n - pt; pt = n; } };
    ++drawsThisFrame;
    {   // RSXSKIP=a-b: skip draws a..b of every frame (debug bisection)
        static long s0 = -1, s1 = -1;
        static bool parsed = false;
        if (!parsed) { parsed = true; if (const char* e = std::getenv("RSXSKIP")) std::sscanf(e, "%ld-%ld", &s0, &s1); }
        if (long(drawsThisFrame) - 1 >= s0 && long(drawsThisFrame) - 1 <= s1) return;
    }
    if (!bind_target()) return;
    apply_scissor();
    // programs
    const uint32_t sp = R[0x8e4 >> 2];
    const uint32_t fpEa = ea_of(sp & ~3u, (sp & 3) == 2 ? 1 : 0);
    uint32_t texDims = 0;
    for (int i = 0; i < 16; ++i) texDims |= texDimsOf(i) << (2 * i);
    Program* p = get_program(R[0x1ea0 >> 2] & 511, fpEa, texDims, R[0x1d60 >> 2]);
    if (!p || p->bad) return;
    mark();
    glUseProgram(p->prog);
    apply_state();
    // vertex program constants
    if (vpConstDirty) { glBindBuffer(GL_UNIFORM_BUFFER, ubo); glBufferData(GL_UNIFORM_BUFFER, sizeof vpConst, vpConst, GL_STREAM_DRAW); vpConstDirty = false; }
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    float vs[4] = {rf(0xa30), rf(0xa34), rf(0xa38), rf(0xa3c)}, vo[4] = {rf(0xa20), rf(0xa24), rf(0xa28), rf(0xa2c)};
    glUniform4fv(p->uVs, 1, vs);
    glUniform4fv(p->uVo, 1, vo);
    const float surf[4] = {float(cur.logW), float(cur.logH), 0, 0};
    glUniform4fv(p->uSurf, 1, surf);
    const uint32_t af = R[0x308 >> 2];
    const float alpha[4] = {float(R[0x304 >> 2] & 1), float(af >= 0x200 ? af - 0x200 : 7), (R[0x30c >> 2] & 0xff) / 255.f, 0};
    glUniform4fv(p->uAlpha, 1, alpha);
    if (p->nConst && p->uFc >= 0) {   // fragment program constants (patched into the microcode by the game)
        glUniform4fv(p->uFc, GLsizei(g_fpConsts.size() / 4), g_fpConsts.data());
    }
    const float wp[4] = {1.f / cur.aaX, 1.f / cur.aaY, 0, 0};
    glUniform4fv(p->uWpos, 1, wp);
    mark();
    float tsc[16 * 4];
    for (int i = 0; i < 16; ++i) { tsc[4 * i] = tsc[4 * i + 1] = tsc[4 * i + 2] = tsc[4 * i + 3] = 1.f; if (p->texUsed & (1u << i)) bind_texture(i, tsc + 4 * i); }
    if (p->uTsc >= 0) glUniform4fv(p->uTsc, 16, tsc);
    mark();
    // vertices
    uint32_t vmin = 0xffffffffu, vmax = 0;
    idx.clear();
    const uint32_t ia = R[0x1820 >> 2];
    const bool i16 = (ia >> 4) & 1;
    const uint32_t ibase = ea_of(R[0x181c >> 2], ia & 1);
    const bool restart = R[0x1dac >> 2] & 1;
    const uint32_t restartIdx = R[0x1db0 >> 2];
    {   // gather the indices of all ranges into idx (0xffffffff = primitive restart): tight loops, this runs for every index of every draw
        const GLenum glp = gl_prim(prim);
        const bool joinStrips = glp != GL_TRIANGLES && glp != GL_LINES && glp != GL_POINTS;
        const uint32_t add = rangesIndexed ? R[0x173c >> 2] : 0;
        size_t total = 0;
        for (const Range& r : ranges) total += size_t(r.count) + 1;
        idx.resize(prim == 8 ? total * 3 / 2 + 8 : total);
        static std::vector<uint32_t> seq;
        uint32_t* o = idx.data();
        for (const Range& r : ranges) {
            if (prim == 8) seq.resize(r.count);
            uint32_t* d = prim == 8 ? seq.data() : o;
            if (!rangesIndexed) for (uint32_t k = 0; k < r.count; ++k) d[k] = r.first + k;
            else if (i16) {
                const uint8_t* q = C.mem + ibase + 2 * size_t(r.first);
                for (uint32_t k = 0; k < r.count; ++k) { uint16_t w; std::memcpy(&w, q + 2 * size_t(k), 2); d[k] = uint32_t(__builtin_bswap16(w)) + add; }
            } else {
                const uint8_t* q = C.mem + ibase + 4 * size_t(r.first);
                for (uint32_t k = 0; k < r.count; ++k) { uint32_t w; std::memcpy(&w, q + 4 * size_t(k), 4); d[k] = __builtin_bswap32(w) + add; }
            }
            if (restart && rangesIndexed) for (uint32_t k = 0; k < r.count; ++k) d[k] = d[k] == restartIdx ? 0xffffffffu : d[k];
            if (prim == 8) {   // quads -> triangles
                for (size_t q = 0; q + 3 < seq.size(); q += 4) { o[0] = seq[q]; o[1] = seq[q + 1]; o[2] = seq[q + 2]; o[3] = seq[q]; o[4] = seq[q + 2]; o[5] = seq[q + 3]; o += 6; }
            } else {
                o += r.count;
                if (&r != &ranges.back() && joinStrips) *o++ = 0xffffffffu;
            }
        }
        idx.resize(size_t(o - idx.data()));
        uint32_t lo = 0xffffffffu, hi1 = 0;   // hi1 = highest index + 1 (the restart value wraps to 0 and drops out)
        for (const uint32_t v : idx) { lo = std::min(lo, v); hi1 = std::max(hi1, v + 1); }
        if (hi1 == 0) return;
        vmin = lo; vmax = hi1 - 1;
    }
    if (vmin > vmax || idx.empty()) return;
    {   // RSXSKIPFMT=xxxx: skip draws whose position array has this format (debug); RSXONLYFMT: draw only those
        static const long sf = std::getenv("RSXSKIPFMT") ? std::strtol(std::getenv("RSXSKIPFMT"), nullptr, 16) : -1;
        static const long of = std::getenv("RSXONLYFMT") ? std::strtol(std::getenv("RSXONLYFMT"), nullptr, 16) : -1;
        const long f0 = long(R[0x1740 >> 2] & 0xffff);
        if (cur.w == 2560 && ((sf >= 0 && f0 == sf) || (of >= 0 && f0 != of))) return;
    }
    mark();
    // A draw often uses a few vertices spread over a large buffer: convert only the referenced ones then.
    static std::vector<uint32_t> uniq, vmap;
    const uint32_t span = vmax - vmin + 1;
    const bool sparse = rangesIndexed && span > 64 && span > idx.size() * 2;
    if (sparse) {
        uniq.clear();
        for (uint32_t v : idx) if (v != 0xffffffffu) uniq.push_back(v);
        std::sort(uniq.begin(), uniq.end());
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    }
    const uint32_t nv = sparse ? uint32_t(uniq.size()) : span;
    if (nv > 4000000) { RLOG("[rsx] draw with %u vertices skipped\n", nv); return; }
    int attrs[16], na = 0;
    for (int a = 0; a < 16; ++a) {
        if (!(p->vpInputs & (1u << a))) continue;
        const uint32_t fmt = R[(0x1740 + 4 * a) >> 2];
        if ((fmt >> 4) & 0xf) attrs[na++] = a;
        else {
            glDisableVertexAttribArray(a);
            glVertexAttrib4f(a, rf(0x1c00 + 16 * a), rf(0x1c04 + 16 * a), rf(0x1c08 + 16 * a), rf(0x1c0c + 16 * a));
        }
    }
    vtx.resize(size_t(na) * nv * 4);
    GLuint streamVbo[16] = {};
    for (int k = 0; k < na; ++k) {
        const uint32_t fmt = R[(0x1740 + 4 * attrs[k]) >> 2];
        const uint32_t stride = (fmt >> 8) & 0xff;
        if (!sparse && (streamVbo[k] = cached_stream(attrs[k], vmax + 1)) != 0) continue;
        if (stride == 0) { convert_attr(attrs[k], 0, 1, vtx.data() + size_t(k) * nv * 4); for (uint32_t v = 1; v < nv; ++v) std::memcpy(&vtx[(size_t(k) * nv + v) * 4], &vtx[size_t(k) * nv * 4], 16); }
        else if (!sparse) convert_attr(attrs[k], vmin, nv, vtx.data() + size_t(k) * nv * 4);
        else
            for (uint32_t i = 0; i < nv;) {   // runs of consecutive vertices
                uint32_t j = i + 1;
                while (j < nv && uniq[j] == uniq[j - 1] + 1) ++j;
                convert_attr(attrs[k], uniq[i], j - i, vtx.data() + (size_t(k) * nv + i) * 4);
                i = j;
            }
    }
    static const bool vstat = std::getenv("RSXVSTAT") != nullptr;
    if (vstat && na) {   // sanity of positions (attribute 0): NaN / huge values, by memory location
        const uint32_t fo = R[0x1680 >> 2];
        uint32_t bad = 0;
        float mx = 0;
        for (uint32_t v = 0; v < nv; ++v)
            for (int c = 0; c < 3; ++c) { const float f = vtx[v * 4 + c]; if (!(std::fabs(f) < 1e5f)) ++bad; else mx = std::max(mx, std::fabs(f)); }
        static uint64_t tot[2] = {}, badv[2] = {}, draws[2] = {}, badDraws[2] = {};
        const int m = fo >> 31;
        tot[m] += nv; badv[m] += bad; ++draws[m]; if (bad) ++badDraws[m];
        if (bad && badDraws[m] < 20) RLOG("[vstat] %s draw: %u of %u position values bad (attr0 fmt %08x off %08x, max ok %g, idx range %u-%u)\n", m ? "main" : "local", bad, nv * 3, R[0x1740 >> 2], fo, mx, vmin, vmax);
        if (frameNo % 100 == 0 && drawsThisFrame == 1) RLOG("[vstat] local: %llu/%llu draws bad; main: %llu/%llu draws bad\n", (unsigned long long)badDraws[0], (unsigned long long)draws[0], (unsigned long long)badDraws[1], (unsigned long long)draws[1]);
    }
    mark();
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vtx.size() * 4, vtx.data(), GL_STREAM_DRAW);
    for (int k = 0; k < na; ++k) {
        glEnableVertexAttribArray(attrs[k]);
        if (streamVbo[k]) {   // cached stream: holds every vertex from 0, the indices are relative to vmin
            glBindBuffer(GL_ARRAY_BUFFER, streamVbo[k]);
            glVertexAttribPointer(attrs[k], 4, GL_FLOAT, 0, 16, reinterpret_cast<const void*>(size_t(vmin) * 16));
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
        } else
            glVertexAttribPointer(attrs[k], 4, GL_FLOAT, 0, 16, reinterpret_cast<const void*>(size_t(k) * nv * 16));
    }
    if (sparse) {
        if (vmap.size() < span) vmap.resize(span);
        for (uint32_t i = 0; i < nv; ++i) vmap[uniq[i] - vmin] = i;
        for (uint32_t& v : idx) if (v != 0xffffffffu) v = vmap[v - vmin];
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STREAM_DRAW);
    // not sparse: the buffers start at vertex vmin, the base vertex takes it off the indices (restart is tested before it)
    glDrawElementsBaseVertex(gl_prim(prim), GLsizei(idx.size()), GL_UNSIGNED_INT, nullptr, sparse ? 0 : -GLint(vmin));
    mark();
    if (prof) {
        ++pfDraws; pfVerts += nv; pfIdx += idx.size();
        const uint64_t now = SDL_GetTicks();
        if (now - pfLast >= 4000) {
            const double k = 1000.0 / double(SDL_GetPerformanceFrequency()) / ((now - pfLast) / 1000.0);
            std::string o; char b[64];
            for (int q = 0; q < 6; ++q) { std::snprintf(b, sizeof b, " %s %.0f,", pfName[q], pfT[q] * k); o += b; pfT[q] = 0; }
            if (pfLast) RLOG("[rsxprof] ms/s:%s | per second: %.0f draws, %.0f k vertices, %.0f k indices\n", o.c_str(), pfDraws * 1000.0 / (now - pfLast), pfVerts / double(now - pfLast), pfIdx / double(now - pfLast));
            if (pfLast) { RLOG("[rsxprof] texture units: %llu memo hits, misses: %llu new, %llu generation, %llu age, %llu sampler state\n", (unsigned long long)g_memoHits, (unsigned long long)g_memoMiss[0], (unsigned long long)g_memoMiss[1], (unsigned long long)g_memoMiss[2], (unsigned long long)g_memoMiss[3]); g_memoHits = 0; for (auto& x : g_memoMiss) x = 0; }
            pfDraws = pfVerts = pfIdx = 0; pfLast = now;
        }
    }
    {   // RSXSTEP=frame: the main surface after every draw of that frame, 640x360 RGB, appended to rsxstep.bin
        static const long stepFrame = std::getenv("RSXSTEP") ? std::atol(std::getenv("RSXSTEP")) : -1;
        if (long(frameNo) == stepFrame && cur.w == 2560 && cur.h == 720) {
            static FILE* sf = std::fopen("rsxstep.bin", "wb");
            static std::vector<uint8_t> full(2560 * 720 * 4), small(640 * 360 * 3);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, cur.fbo);
            glReadPixels(0, 0, 2560, 720, GL_RGBA, GL_UNSIGNED_BYTE, full.data());
            for (int y = 0; y < 360; ++y)
                for (int x = 0; x < 640; ++x)
                    std::memcpy(&small[(y * 640 + x) * 3], &full[((y * 2) * 2560 + x * 4) * 4], 3);
            if (sf) { const uint32_t n = uint32_t(drawsThisFrame - 1); std::fwrite(&n, 4, 1, sf); std::fwrite(small.data(), 1, small.size(), sf); std::fflush(sf); }
        }
    }
    static const long logFrame = std::getenv("RSXLOG") ? std::atol(std::getenv("RSXLOG")) : -1;
    if (long(frameNo) == logFrame) {
        RLOG("[draw] #%llu prog %016llx fb %u (%ux%u, color %x z %x fmt %08x ct %x) prim %u idx %zu verts %u attrs %d vp %u fp %x tex %x | blend %u depth %u/%x mask %x cull %u alpha %u | pos0 %g %g %g %g\n",
             (unsigned long long)drawsThisFrame - 1, (unsigned long long)p->key, cur.fbo, cur.w, cur.h, R[0x210 >> 2], R[0x214 >> 2], R[0x208 >> 2], R[0x220 >> 2], prim, idx.size(), nv, na, R[0x1ea0 >> 2], fpEa, p->texUsed,
             R[0x310 >> 2], R[0xa74 >> 2], R[0xa6c >> 2], R[0x324 >> 2], R[0x183c >> 2], R[0x304 >> 2], na ? vtx[0] : 0.f, na ? vtx[1] : 0.f, na ? vtx[2] : 0.f, na ? vtx[3] : 0.f);
        for (int i = 0; i < 16; ++i) if (p->texUsed & (1u << i)) {
            const uint32_t base = i < 8 ? 0x1a00 + 0x20 * i : 0x1b00 + 0x20 * (i - 8);
            RLOG("[draw]   tex %d off %x fmt %08x addr %08x ctl0 %08x remap %08x filt %08x rect %08x ctl3 %08x\n", i, R[base >> 2], R[(base + 4) >> 2], R[(base + 8) >> 2],
                 R[(base + 0xc) >> 2], R[(base + 0x10) >> 2], R[(base + 0x14) >> 2], R[(base + 0x18) >> 2], R[(0x1840 + 4 * i) >> 2]);
        }
        {
            uint8_t px[4] = {};
            glBindFramebuffer(GL_READ_FRAMEBUFFER, cur.fbo);
            glReadPixels(cur.w / 2, cur.h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            uint8_t c0[4] = {}, c1[4] = {}, c2[4] = {};
            glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c0);
            glReadPixels(cur.w - 2, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c1);
            glReadPixels(1, cur.h - 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c2);
            RLOG("[draw]   corners: %u %u %u %u | %u %u %u %u | %u %u %u %u\n", c0[0], c0[1], c0[2], c0[3], c1[0], c1[1], c1[2], c1[3], c2[0], c2[1], c2[2], c2[3]);
            std::string is; char ib[16];
            for (size_t q = 0; q < idx.size() && q < 12; ++q) { std::snprintf(ib, sizeof ib, " %u", idx[q]); is += ib; }
            RLOG("[draw]   indices:%s (ranges %zu, first %u count %u, indexed %d)\n", is.c_str(), ranges.size(), ranges[0].first, ranges[0].count, int(rangesIndexed));
            GLint sb[4] = {}, vp[4] = {}, cmk[4] = {}, fb = 0, dfb = 0;
            glGetIntegerv(0x0C10, sb); glGetIntegerv(0x0BA2, vp); glGetIntegerv(0x0C23, cmk); glGetIntegerv(0x8CA6, &dfb); glGetIntegerv(0x8CAA, &fb);
            RLOG("[draw]   GL scissor %d %d %d %d (on %d) viewport %d %d %d %d colormask %d %d %d %d drawfb %d readfb %d depth %d stencil %d cull %d blend %d rd %d\n", sb[0], sb[1], sb[2], sb[3], glIsEnabled(GL_SCISSOR_TEST),
                 vp[0], vp[1], vp[2], vp[3], cmk[0], cmk[1], cmk[2], cmk[3], dfb, fb, glIsEnabled(GL_DEPTH_TEST), glIsEnabled(GL_STENCIL_TEST), glIsEnabled(GL_CULL_FACE), glIsEnabled(GL_BLEND), glIsEnabled(0x8C89));
            RLOG("[draw]   center pixel after draw: %u %u %u %u, GL error %x, stencil %u depthbounds %x\n", px[0], px[1], px[2], px[3], glGetError(), R[0x328 >> 2], R[0x380 >> 2]);
        }
        RLOG("[draw]   scissor %08x %08x viewport %08x %08x clip %08x %08x window %08x ctl %08x\n", R[0x8c0 >> 2], R[0x8c4 >> 2], R[0xa00 >> 2], R[0xa04 >> 2], R[0x200 >> 2], R[0x204 >> 2], R[0x1d88 >> 2], R[0x1d60 >> 2]);
        for (uint32_t v = 0; v < nv && v < 6; ++v) RLOG("[draw]   v%u: %g %g %g %g | %g %g\n", v, vtx[v * 4], vtx[v * 4 + 1], vtx[v * 4 + 2], vtx[v * 4 + 3], na > 2 ? vtx[(2 * nv + v) * 4] : 0.f, na > 2 ? vtx[(2 * nv + v) * 4 + 1] : 0.f);
        RLOG("[draw]   vc467 %g %g %g %g vc4 %g %g %g %g vscale %g %g %g voff %g %g %g\n", vpConst[467 * 4], vpConst[467 * 4 + 1], vpConst[467 * 4 + 2], vpConst[467 * 4 + 3],
             vpConst[16], vpConst[17], vpConst[18], vpConst[19], vs[0], vs[1], vs[2], vo[0], vo[1], vo[2]);
        for (int k = 0; k < na; ++k) {
            const uint32_t fo = R[(0x1680 + 4 * attrs[k]) >> 2];
            const uint32_t e = ea_of(fo & 0x7fffffff, fo >> 31) + vmin * ((R[(0x1740 + 4 * attrs[k]) >> 2] >> 8) & 0xff);
            RLOG("[draw]   attr %d fmt %08x off %08x raw %08x %08x %08x %08x -> %g %g %g %g\n", attrs[k], R[(0x1740 + 4 * attrs[k]) >> 2], fo, be32(e), be32(e + 4), be32(e + 8), be32(e + 12),
                 vtx[size_t(k) * nv * 4], vtx[size_t(k) * nv * 4 + 1], vtx[size_t(k) * nv * 4 + 2], vtx[size_t(k) * nv * 4 + 3]);
        }
    }
    for (int k = 0; k < na; ++k) glDisableVertexAttribArray(attrs[k]);
}

// ------------------------------------------------------------------ window
static void init_gl() {
    initTried = true;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { RLOG("[rsx] SDL_Init failed: %s\n", SDL_GetError()); return; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    win = SDL_CreateWindow("GTI Club (F1: opciones)", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!win) { RLOG("[rsx] window: %s\n", SDL_GetError()); return; }
    glc = SDL_GL_CreateContext(win);
    if (!glc) { RLOG("[rsx] GL context: %s\n", SDL_GetError()); return; }
    SDL_GL_SetSwapInterval(settings().vsync);
    if (settings().fullscreen) set_display_mode(settings().fullscreen);
#define RSX_LOAD(ret, name, ...) name = reinterpret_cast<PFN_##name>(SDL_GL_GetProcAddress(#name)); if (!name) { RLOG("[rsx] missing GL function %s\n", #name); return; }
    RSX_GL(RSX_LOAD)
#undef RSX_LOAD
    RLOG("[rsx] OpenGL %s on %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    glGenBuffers(1, &vbo); glGenBuffers(1, &ibo); glGenBuffers(1, &ubo);
    glEnable(0x8D69 /* GL_PRIMITIVE_RESTART_FIXED_INDEX */);
    glEnable(GL_PROGRAM_POINT_SIZE);
    if (std::getenv("RSXCLAMP")) glEnable(GL_DEPTH_CLAMP);
    if (dumpShaders) SDL_CreateDirectory("rsxdump");
    glOk = true;
}

// Display mode: 0 window, 1 borderless window covering the monitor, 2 full screen.
// Full screen (SDL's desktop full screen) lets the NVIDIA OpenGL driver show the image directly ("independent flip"):
// lowest latency, but the desktop compositor never sees it, so screen capture (OBS) records black. The borderless
// mode covers the monitor plus one row, which keeps the window composed: it looks the same and can be captured.
static void set_display_mode(int mode) {
    static int sx = 0, sy = 0, sw = 1280, sh = 720;
    static int cur = 0;
    if (cur == 0 && mode != 0) { SDL_GetWindowPosition(win, &sx, &sy); SDL_GetWindowSize(win, &sw, &sh); }
    if (cur == 2) SDL_SetWindowFullscreen(win, false);
    if (cur != 0) { SDL_SetWindowBordered(win, true); SDL_SetWindowSize(win, sw, sh); SDL_SetWindowPosition(win, sx, sy); }
    if (mode == 1) {
        SDL_Rect r{};
        if (SDL_GetDisplayBounds(SDL_GetDisplayForWindow(win), &r)) {
            SDL_SetWindowBordered(win, false);
            SDL_SetWindowPosition(win, r.x, r.y);
            SDL_SetWindowSize(win, r.w, r.h + 1);
        }
    } else if (mode == 2) {
        SDL_SetWindowFullscreen(win, true);
    }
    if (mode != 0) SDL_RaiseWindow(win);
    if (mode != 0 && std::getenv("FSTOP")) SDL_SetWindowAlwaysOnTop(win, true);   // debug: tests started from a background process
    cur = mode;
}
// ------------------------------------------------------------------ options menu (F1)
static bool menuOpen = std::getenv("MENUTEST") != nullptr, menuDirty = true;   // MENUTEST=1 (debug): open from the start
static int menuSel = 1;
static const int kMenuLines = 15, kMenuFirst = 1, kMenuLast = 12;   // title, 12 options, 2 notes
static void menu_change(int dir) {
    Settings& s = settings();
    switch (menuSel) {
        case 1: s.fullscreen = (s.fullscreen + (dir ? dir : 1) + 3) % 3; if (s.fullscreen) g_lastFull = s.fullscreen; set_display_mode(s.fullscreen); break;
        case 2: s.scale = std::clamp(s.scale + dir, 1, 3); break;
        case 3: { static const int r[4] = {60, 72, 120, 144}; int i = 0; while (i < 3 && r[i] != s.gameHz) ++i; s.gameHz = r[(i + dir + 4) % 4]; break; }
        case 4: s.refresh ^= 1; break;
        case 5: s.language = 1 + (s.language - 1 + dir + 5) % 5; break;
        case 6: s.showFps ^= 1; if (!s.showFps) SDL_SetWindowTitle(win, "GTI Club"); break;
        case 7: s.vsync ^= 1; SDL_GL_SetSwapInterval(s.vsync); break;
        case 8: s.sound ^= 1; break;
        case 9: s.aniso = std::clamp(s.aniso + (dir ? dir : 1), 0, 4); ++g_texGen; break;
        case 10: s.streaming ^= 1; break;
        case 11: s.fxaa ^= 1; break;
        case 12: s.sharpen = (s.sharpen + (dir ? dir : 1) + 4) % 4; break;   // the texture cache keeps sampler state: rebuild it
    }
    menuDirty = true;
}
static void draw_menu(int ww, int wh) {
    static GLuint tex = 0, fbo = 0;
    const int W = 880, H = 800;
    if (!tex) {
        glGenTextures(1, &tex); glGenFramebuffers(1, &fbo);
        glActiveTexture(GL_TEXTURE0 + 15); ++g_texGen;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        menuDirty = true;
    }
    if (menuDirty) {
        menuDirty = false;
        const Settings& s = settings();
        static const wchar_t* const res[3] = {L"1280 x 720 (original)", L"2560 x 1440", L"3840 x 2160"};
        static const wchar_t* const lang[5] = {L"English", L"Français", L"Español", L"Deutsch", L"Italiano"};
        wchar_t l[kMenuLines][96];
        std::swprintf(l[0], 96, L"OPCIONES");
        std::swprintf(l[1], 96, L"Modo de pantalla\t‹ %ls ›", s.fullscreen == 0 ? L"Ventana" : s.fullscreen == 1 ? L"Ventana sin bordes" : L"Pantalla completa");
        std::swprintf(l[2], 96, L"Resolución interna\t‹ %ls ›", res[std::clamp(s.scale, 1, 3) - 1]);
        std::swprintf(l[3], 96, L"Imágenes por segundo *\t‹ %ls ›", s.gameHz == 60 ? L"60 (original)" : s.gameHz == 72 ? L"72 (ideal en 144 Hz)" : s.gameHz == 120 ? L"120 (experimental)" : L"144 (experimental)");
        std::swprintf(l[4], 96, L"Adaptar la pantalla al juego *\t‹ %ls ›", s.refresh ? L"Sí" : L"No");
        std::swprintf(l[5], 96, L"Idioma del juego *\t‹ %ls ›", lang[std::clamp(s.language, 1, 5) - 1]);
        std::swprintf(l[6], 96, L"Contador de FPS en el título\t‹ %ls ›", s.showFps ? L"Sí" : L"No");
        std::swprintf(l[7], 96, L"V-Sync\t‹ %ls ›", s.vsync ? L"Sí" : L"No");
        std::swprintf(l[8], 96, L"Sonido *\t‹ %ls ›", s.sound ? L"Sí" : L"No");
        {
            static const wchar_t* const an[5] = {L"No", L"2x", L"4x", L"8x", L"16x"};
            std::swprintf(l[9], 96, L"Filtrado anisótropo\t‹ %ls ›", an[std::clamp(s.aniso, 0, 4)]);
        }
        std::swprintf(l[10], 96, L"Modo directo (60 para OBS/Twitch) *\t‹ %ls ›", s.streaming ? L"Sí" : L"No");
        std::swprintf(l[11], 96, L"Antialiasing (FXAA)\t‹ %ls ›", s.fxaa ? L"Sí" : L"No");
        {
            static const wchar_t* const sh[4] = {L"No", L"Baja", L"Media", L"Alta"};
            std::swprintf(l[12], 96, L"Nitidez\t‹ %ls ›", sh[std::clamp(s.sharpen, 0, 3)]);
        }
        std::swprintf(l[13], 96, L"* se aplica al reiniciar el juego");
        std::swprintf(l[14], 96, L"Flechas: elegir y cambiar   ·   F1 o Esc: cerrar   ·   F11: ventana / pantalla completa");
        const wchar_t* lines[kMenuLines];
        for (int i = 0; i < kMenuLines; ++i) lines[i] = l[i];
        static std::vector<uint32_t> px;
        px.resize(size_t(W) * H);
        if (!host_menu_bitmap(lines, kMenuLines, menuSel, W, H, px.data())) return;
        glActiveTexture(GL_TEXTURE0 + 15); ++g_texGen;
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(0x0CF5 /* GL_UNPACK_ALIGNMENT */, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, 0x8058 /* GL_RGBA8 */, W, H, 0, 0x80E1 /* GL_BGRA */, GL_UNSIGNED_BYTE, px.data());
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    const int dh = std::min(wh * 7 / 10, H * 2), dw = dh * W / H, dx = (ww - dw) / 2, dy = (wh - dh) / 2;
    glBlitFramebuffer(0, 0, W, H, dx, dy + dh, dx + dw, dy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

static SDL_Gamepad* pad = nullptr;
static void pump_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) quitFlag = true;
        if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) pad = SDL_OpenGamepad(e.gdevice.which);
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            const SDL_Scancode sc = e.key.scancode;
            if (sc == SDL_SCANCODE_F11 || (sc == SDL_SCANCODE_RETURN && (e.key.mod & SDL_KMOD_ALT))) { Settings& st = settings(); if (st.fullscreen) { g_lastFull = st.fullscreen; st.fullscreen = 0; } else st.fullscreen = g_lastFull; set_display_mode(st.fullscreen); menuDirty = true; settings_save(); }
            else if (sc == SDL_SCANCODE_F1 || (menuOpen && sc == SDL_SCANCODE_ESCAPE)) { menuOpen = !menuOpen; menuDirty = true; if (!menuOpen) settings_save(); }
            else if (menuOpen && sc == SDL_SCANCODE_UP) { menuSel = menuSel > kMenuFirst ? menuSel - 1 : kMenuLast; menuDirty = true; }
            else if (menuOpen && sc == SDL_SCANCODE_DOWN) { menuSel = menuSel < kMenuLast ? menuSel + 1 : kMenuFirst; menuDirty = true; }
            else if (menuOpen && sc == SDL_SCANCODE_LEFT) menu_change(-1);
            else if (menuOpen && (sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_SPACE)) menu_change(1);
        }
    }
    {   // vibration the game asks for (cellPadSetActDirect), renewed while it lasts
        static uint32_t last = 0; static uint64_t renewed = 0;
        const uint32_t rb = menuOpen || std::getenv("NORUMBLE") ? 0 : lv2_rumble();
        const uint64_t now = SDL_GetTicks();
        if (pad && (rb != last || (rb && now - renewed > 150))) {
            const uint16_t low = uint16_t((rb & 0xff) * 257), high = (rb >> 8) ? 0xffff : 0;
            SDL_RumbleGamepad(pad, low, high, rb ? 300 : 0);
            last = rb; renewed = now;
        }
    }
    if (menuOpen) { lv2_set_pad(PadState{}); return; }   // the game sees no input while the menu is open
    // input -> DualShock 3 state
    PadState s;
    const bool* k = SDL_GetKeyboardState(nullptr);
    auto btn = [&](SDL_GamepadButton b) { return pad && SDL_GetGamepadButton(pad, b); };
    auto axis = [&](SDL_GamepadAxis a) { return pad ? SDL_GetGamepadAxis(pad, a) / 32768.f : 0.f; };
    if (k[SDL_SCANCODE_RETURN] || btn(SDL_GAMEPAD_BUTTON_START)) s.digital1 |= 0x08;
    if (k[SDL_SCANCODE_BACKSPACE] || btn(SDL_GAMEPAD_BUTTON_BACK)) s.digital1 |= 0x01;
    if (k[SDL_SCANCODE_UP] || btn(SDL_GAMEPAD_BUTTON_DPAD_UP)) s.digital1 |= 0x10;
    if (k[SDL_SCANCODE_RIGHT] || btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) s.digital1 |= 0x20;
    if (k[SDL_SCANCODE_DOWN] || btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) s.digital1 |= 0x40;
    if (k[SDL_SCANCODE_LEFT] || btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) s.digital1 |= 0x80;
    const float lt = std::max(axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), k[SDL_SCANCODE_S] ? 1.f : 0.f);
    const float rt = std::max(axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER), k[SDL_SCANCODE_W] ? 1.f : 0.f);
    if (lt > 0.1f) s.digital2 |= 0x01;
    if (rt > 0.1f) s.digital2 |= 0x02;
    if (k[SDL_SCANCODE_Q] || btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) s.digital2 |= 0x04;
    if (k[SDL_SCANCODE_E] || btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) s.digital2 |= 0x08;
    if (k[SDL_SCANCODE_V] || btn(SDL_GAMEPAD_BUTTON_NORTH)) s.digital2 |= 0x10;
    if (k[SDL_SCANCODE_ESCAPE] || btn(SDL_GAMEPAD_BUTTON_EAST)) s.digital2 |= 0x20;
    if (k[SDL_SCANCODE_X] || k[SDL_SCANCODE_SPACE] || btn(SDL_GAMEPAD_BUTTON_SOUTH)) s.digital2 |= 0x40;
    if (k[SDL_SCANCODE_C] || btn(SDL_GAMEPAD_BUTTON_WEST)) s.digital2 |= 0x80;
    if (k[SDL_SCANCODE_L] || btn(SDL_GAMEPAD_BUTTON_LEFT_STICK)) s.digital1 |= 0x02;    // L3: headlights
    if (k[SDL_SCANCODE_H] || btn(SDL_GAMEPAD_BUTTON_RIGHT_STICK)) s.digital1 |= 0x04;   // R3: horn
    float lx = axis(SDL_GAMEPAD_AXIS_LEFTX), ly = axis(SDL_GAMEPAD_AXIS_LEFTY);
    if (k[SDL_SCANCODE_A]) lx = -1; if (k[SDL_SCANCODE_D]) lx = 1;
    auto a8 = [](float v) { return uint16_t(std::clamp(int(128 + v * 127.5f), 0, 255)); };
    s.analog[0] = a8(axis(SDL_GAMEPAD_AXIS_RIGHTX)); s.analog[1] = a8(axis(SDL_GAMEPAD_AXIS_RIGHTY));
    s.analog[2] = a8(lx); s.analog[3] = a8(ly);
    // pressure order: right, left, up, down, triangle, circle, cross, square, L1, R1, L2, R2
    auto pr = [](bool b) { return uint16_t(b ? 255 : 0); };
    s.pressure[0] = pr(s.digital1 & 0x20); s.pressure[1] = pr(s.digital1 & 0x80); s.pressure[2] = pr(s.digital1 & 0x10); s.pressure[3] = pr(s.digital1 & 0x40);
    s.pressure[4] = pr(s.digital2 & 0x10); s.pressure[5] = pr(s.digital2 & 0x20); s.pressure[6] = pr(s.digital2 & 0x40); s.pressure[7] = pr(s.digital2 & 0x80);
    s.pressure[8] = pr(s.digital2 & 0x04); s.pressure[9] = pr(s.digital2 & 0x08);
    s.pressure[10] = uint16_t(std::clamp(lt, 0.f, 1.f) * 255); s.pressure[11] = uint16_t(std::clamp(rt, 0.f, 1.f) * 255);
    static bool any = false;
    if (s.digital1 || s.digital2 || std::abs(lx) > 0.2f || std::abs(ly) > 0.2f) any = true;
    if (any) lv2_set_pad(s);   // the scripted test input stays active until the user touches something
}

void init(const Config& cfg) { C = cfg; }

void set_display_buffer(uint32_t id, uint32_t offset, uint32_t pitch, uint32_t w, uint32_t h) {
    if (id < 8) displays[id] = Display{offset, pitch, w, h};
    RLOG("[rsx] display buffer %u: offset 0x%x pitch %u %ux%u\n", id, offset, pitch, w, h);
}

// ------------------------------------------------------------------ final image: antialiasing and sharpening
// Optional passes between the game's display buffer and the window (menu F1). FXAA: luminance edges are blurred
// along their direction (the classic "FXAA console" scheme), in the display buffer's own resolution.
// Sharpening: contrast adaptive (strong where the neighbourhood is flat, weak near strong contrast so edges do not
// ring), applied while scaling to the window. Without either the display buffer is just blitted.
static const char* const kPostVs =
    "#version 450 core\n"
    "out vec2 uv;\n"
    "void main() { vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
static const char* const kFxaaFs =
    "#version 450 core\n"
    "layout(binding = 15) uniform sampler2D src;\n"
    "uniform vec4 u_p;\n"
    "in vec2 uv; out vec4 o;\n"
    "float lum(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "void main() {\n"
    "  vec2 rcp = u_p.xy;\n"
    "  vec3 m = texture(src, uv).rgb;\n"
    "  float lM = lum(m);\n"
    "  float lNW = lum(texture(src, uv + vec2(-1.0, 1.0) * rcp).rgb), lNE = lum(texture(src, uv + vec2(1.0, 1.0) * rcp).rgb);\n"
    "  float lSW = lum(texture(src, uv + vec2(-1.0, -1.0) * rcp).rgb), lSE = lum(texture(src, uv + vec2(1.0, -1.0) * rcp).rgb);\n"
    "  float mn = min(lM, min(min(lNW, lNE), min(lSW, lSE))), mx = max(lM, max(max(lNW, lNE), max(lSW, lSE)));\n"
    "  if (mx - mn < max(0.0312, mx * 0.125)) { o = vec4(m, 1.0); return; }\n"
    "  vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));\n"
    "  float reduce = max((lNW + lNE + lSW + lSE) * 0.03125, 1.0 / 128.0);\n"
    "  dir = clamp(dir / (min(abs(dir.x), abs(dir.y)) + reduce), vec2(-8.0), vec2(8.0)) * rcp;\n"
    "  vec3 a = 0.5 * (texture(src, uv + dir * (1.0 / 3.0 - 0.5)).rgb + texture(src, uv + dir * (2.0 / 3.0 - 0.5)).rgb);\n"
    "  vec3 b = a * 0.5 + 0.25 * (texture(src, uv - dir * 0.5).rgb + texture(src, uv + dir * 0.5).rgb);\n"
    "  float lB = lum(b);\n"
    "  o = vec4((lB < mn || lB > mx) ? a : b, 1.0);\n"
    "}\n";
static const char* const kSharpFs =
    "#version 450 core\n"
    "layout(binding = 15) uniform sampler2D src;\n"
    "uniform vec4 u_p;\n"
    "in vec2 uv; out vec4 o;\n"
    "void main() {\n"
    "  vec2 rcp = u_p.xy; vec2 u = vec2(uv.x, 1.0 - uv.y);\n"   // display buffers are stored upside down
    "  vec3 c = texture(src, u).rgb;\n"
    "  if (u_p.z <= 0.0) { o = vec4(c, 1.0); return; }\n"
    "  vec3 n = texture(src, u + vec2(0.0, rcp.y)).rgb, s = texture(src, u - vec2(0.0, rcp.y)).rgb;\n"
    "  vec3 e = texture(src, u + vec2(rcp.x, 0.0)).rgb, w = texture(src, u - vec2(rcp.x, 0.0)).rgb;\n"
    "  vec3 mn = min(c, min(min(n, s), min(e, w))), mx = max(c, max(max(n, s), max(e, w)));\n"
    "  vec3 amp = sqrt(clamp(min(mn, 1.0 - mx) / max(mx, vec3(1e-5)), 0.0, 1.0));\n"
    "  vec3 wgt = amp * (-1.0 / mix(8.0, 5.0, u_p.z));\n"
    "  o = vec4(clamp((c + (n + s + e + w) * wgt) / (1.0 + 4.0 * wgt), 0.0, 1.0), 1.0);\n"
    "}\n";
static GLuint post_program(const char* fs) {
    const GLuint p = glCreateProgram();
    const GLuint v = compile(GL_VERTEX_SHADER, kPostVs, "post vertex"), f = compile(GL_FRAGMENT_SHADER, fs, "post fragment");
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { RLOG("[rsx] post program failed to link\n"); return 0; }
    return p;
}
// Draws the display buffer `src` (sw x sh) into the window rectangle; false when the passes are off or unusable
static bool post_present(GLuint src, int sw, int sh, int dx, int dy, int dw, int dh) {
    const Settings& st = settings();
    if (!st.fxaa && !st.sharpen) return false;
    static GLuint fxaaProg = 0, sharpProg = 0, mid = 0, midFbo = 0, pvao = 0;
    static GLint fxaaP = -1, sharpP = -1;
    static int midW = 0, midH = 0;
    static bool tried = false;
    if (!tried) {
        tried = true;
        fxaaProg = post_program(kFxaaFs); sharpProg = post_program(kSharpFs);
        if (fxaaProg) fxaaP = glGetUniformLocation(fxaaProg, "u_p");
        if (sharpProg) sharpP = glGetUniformLocation(sharpProg, "u_p");
        glGenVertexArrays(1, &pvao); glGenFramebuffers(1, &midFbo);
    }
    if (!fxaaProg || !sharpProg) return false;
    ++g_texGen;   // unit 15 is rebound here
    glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(pvao);
    glActiveTexture(GL_TEXTURE0 + 15);
    GLuint img = src;
    if (st.fxaa) {
        if (!mid || midW != sw || midH != sh) {
            if (!mid) glGenTextures(1, &mid);
            glBindTexture(GL_TEXTURE_2D, mid);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sw, sh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, midFbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mid, 0);
            const GLenum b0 = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &b0);
            midW = sw; midH = sh;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, midFbo);
        glViewport(0, 0, sw, sh);
        glBindTexture(GL_TEXTURE_2D, src);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUseProgram(fxaaProg);
        const float p[4] = {1.f / sw, 1.f / sh, 0, 0};
        glUniform4fv(fxaaP, 1, p);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        img = mid;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(dx, dy, dw, dh);
    glBindTexture(GL_TEXTURE_2D, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUseProgram(sharpProg);
    static const float amount[4] = {0.f, 0.3f, 0.6f, 1.f};
    const float p[4] = {1.f / sw, 1.f / sh, amount[std::clamp(st.sharpen, 0, 3)], 0};
    glUniform4fv(sharpP, 1, p);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glViewport(0, 0, dw, dh);
    return true;
}

void flip(uint32_t id) {
    if (!enabled()) return;
    if (!initTried) init_gl();
    if (!glOk) return;
    ++frameNo;
    stateValid = false;
    {   // frame pacing: longest gap between two flips, reported every 2 s (a hitch shows here, not in the average)
        static uint64_t lastFlip = 0, worst = 0, since = 0; static int over = 0;
        const uint64_t t = SDL_GetPerformanceCounter(), fq = SDL_GetPerformanceFrequency();
        if (lastFlip) { const uint64_t d = t - lastFlip; worst = std::max(worst, d); if (d * 1000 > fq * 25) ++over; }
        if (lastFlip && (t - lastFlip) * 1000 > fq * 60) RLOG("[hitch] %.0f ms without a flip, ending %.2f s after launch\n", (t - lastFlip) * 1000.0 / fq, SDL_GetTicks() / 1000.0);
        lastFlip = t;
        if (t - since > 2 * fq) {
            if (since) RLOG("[pace] worst frame %.0f ms, %d frames over 25 ms\n", worst * 1000.0 / fq, over);
            since = t; worst = 0; over = 0;
        }
    }
    const Display& d = displays[id & 7];
    int ww = 1280, wh = 720;
    SDL_GetWindowSizeInPixels(win, &ww, &wh);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    for (int k = 0; k < 4; ++k) glColorMaski(k, 1, 1, 1, 1);
    glViewport(0, 0, ww, wh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    auto it = surfaces.find(uint64_t(d.offset));
    if (it != surfaces.end()) {
        static GLuint readFbo = 0;
        if (!readFbo) glGenFramebuffers(1, &readFbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, it->second.tex, 0);
        // keep the aspect ratio
        const float sa = float(d.w ? d.w : it->second.w) / float(d.h ? d.h : it->second.h), wa = float(ww) / float(wh);
        int dw = ww, dh = wh;
        if (wa > sa) dw = int(wh * sa); else dh = int(ww / sa);
        const int dx = (ww - dw) / 2, dy = (wh - dh) / 2;
        if (!post_present(it->second.tex, int(it->second.w), int(it->second.h), dx, dy, dw, dh))
            glBlitFramebuffer(0, 0, it->second.w, it->second.h, dx, dy + dh, dx + dw, dy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);   // the post passes bind GL_FRAMEBUFFER (the screenshots below read through readFbo)
        static const int shotEvery = std::getenv("RSXSHOT") ? std::atoi(std::getenv("RSXSHOT")) : 0;
        if (shotEvery > 0 && frameNo % shotEvery == 0) {   // RSXSHOT=N: save every color surface every N frames
            RLOG("[shot] frame %llu at %.2f s\n", (unsigned long long)frameNo, SDL_GetTicks() / 1000.0);
            for (auto& sf : surfaces) {
                if (sf.second.depth && std::getenv("RSXSHOTZ") && sf.second.w >= 512 && sf.second.w == sf.second.h) {   // RSXSHOTZ=1: square depth surfaces (shadow maps) as raw floats
                    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
                    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, 0x821A /* DEPTH_STENCIL_ATTACHMENT */, GL_TEXTURE_2D, sf.second.tex, 0);
                    std::vector<float> z(size_t(sf.second.w) * sf.second.h);
                    glReadPixels(0, 0, int(sf.second.w), int(sf.second.h), GL_DEPTH_COMPONENT, 0x1406 /* FLOAT */, z.data());
                    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, 0x821A, GL_TEXTURE_2D, 0, 0);
                    char fn[96];
                    std::snprintf(fn, sizeof fn, "rsxshot_%05llu_Z%x_%u.f32", (unsigned long long)frameNo, uint32_t(sf.first), sf.second.w);
                    if (FILE* zf = std::fopen(fn, "wb")) { std::fwrite(z.data(), 4, z.size(), zf); std::fclose(zf); }
                    continue;
                }
                if (sf.second.depth) continue;
                if (std::getenv("RSXSHOTD") && sf.first != uint64_t(d.offset)) continue;   // RSXSHOTD=1: the display buffer only
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sf.second.tex, 0);
                const int sw = int(sf.second.w), sh = int(sf.second.h);
                std::vector<uint8_t> px(size_t(sw) * sh * 4);
                glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                SDL_Surface* srf = SDL_CreateSurfaceFrom(sw, sh, SDL_PIXELFORMAT_ABGR8888, px.data(), sw * 4);
                char fn[96];
                std::snprintf(fn, sizeof fn, "rsxshot_%05llu_%s%x.bmp", (unsigned long long)frameNo, sf.first == uint64_t(d.offset) ? "DISPLAY_" : "", uint32_t(sf.first));
                if (srf) { SDL_SaveBMP(srf, fn); SDL_DestroySurface(srf); }
            }
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    } else {
        static int warned = 0;
        if (warned++ < 5) RLOG("[rsx] flip %u: display buffer at 0x%x was never rendered\n", id, d.offset);
    }
    if (menuOpen) draw_menu(ww, wh);
    {   // SWAPLOG=1: how long presenting takes (it blocks when the compositor or the driver throttles)
        static const bool swapLog = std::getenv("SWAPLOG") != nullptr;
        const uint64_t ts = swapLog ? SDL_GetTicksNS() : 0;
        SDL_GL_SwapWindow(win);
        if (swapLog) { static uint64_t sum = 0, mx = 0, n = 0; const uint64_t d = SDL_GetTicksNS() - ts; sum += d; mx = std::max(mx, d); if (++n == 240) { RLOG("[swap] 240 presents: %.2f ms average, %.2f ms longest\n", sum / 240e6, mx / 1e6); sum = mx = n = 0; } }
    }
    pump_events();
    static uint64_t lastT = SDL_GetTicks(), lastF = 0;
    const uint64_t now = SDL_GetTicks();
    if (now - lastT >= 2000 && !settings().showFps) { lastT = now; lastF = frameNo; }
    if (now - lastT >= 2000) {
        char title[160];
        std::snprintf(title, sizeof title, "GTI Club - %.1f fps (F1: opciones)", (frameNo - lastF) * 1000.0 / (now - lastT),
                      (unsigned long long)drawsThisFrame, programs.size());
        SDL_SetWindowTitle(win, title);
        lastT = now; lastF = frameNo;
    }
    drawsThisFrame = 0;
    if (const GLenum e = glGetError()) { static int n = 0; if (n++ < 20) RLOG("[rsx] GL error 0x%x (frame %llu)\n", e, (unsigned long long)frameNo); }
}

void method(uint32_t m, uint32_t arg) {
    if (!enabled()) return;
    if (m >= 0x10000) return;
    if (!initTried) init_gl();
    if (!glOk) { R[m >> 2] = arg; return; }
    if (rsxTrace) RLOG("[rsxm] %04x = %08x\n", m, arg);
    if (m >= 0x0b80 && m < 0x0c00) {   // transform program words
        if (vpLoad < 512 * 4) { vpCode[vpLoad++] = arg; ++vpGen; }
        return;
    }
    if (m >= 0x1f00 && m < 0x1f80) {   // transform constants
        const uint32_t i = cLoad * 4 + vpConstWord;
        if (i < 468 * 4) { std::memcpy(&vpConst[i], &arg, 4); vpConstDirty = true; }
        if (++vpConstWord == 4) { vpConstWord = 0; ++cLoad; }
        return;
    }
    switch (m) {
        case 0x1e9c: vpLoad = (arg & 511) * 4; break;
        case 0x1efc: cLoad = arg; vpConstWord = 0; break;
        case 0x1808:   // SET_BEGIN_END
            if (arg) { prim = arg; ranges.clear(); }
            else { do_draw(); ranges.clear(); }
            break;
        case 0x1814: ranges.push_back({arg & 0xffffff, (arg >> 24) + 1}); rangesIndexed = false; break;   // DRAW_ARRAYS
        case 0x1824: ranges.push_back({arg & 0xffffff, (arg >> 24) + 1}); rangesIndexed = true; break;    // DRAW_INDEX_ARRAY
        case 0x1d94: {
            clear_surface(arg);
            static const long logFrame = std::getenv("RSXLOG") ? std::atol(std::getenv("RSXLOG")) : -1;
            if (long(frameNo) == logFrame) RLOG("[clear] %x fb %u color %x (value %08x) z %x\n", arg, cur.fbo, R[0x210 >> 2], R[0x1d90 >> 2], R[0x214 >> 2]);
            break;
        }
        default: break;
    }
    R[m >> 2] = arg;
}

} // namespace gti::recomp::rsx
