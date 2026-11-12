#include "render/renderer.h"
#include "formats/gcm_texture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>

namespace gti::render {

namespace {

const char* kVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aCol;
layout(location=4) in vec2 aUVLM;
layout(location=5) in vec2 aUVOV;
layout(location=6) in vec2 aCorner;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vPos; out vec3 vNrm; out vec2 vUV; out vec4 vCol; out vec2 vUVLM; out vec2 vUVOV;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    wp.xyz += uCamRight * aCorner.x + uCamUp * aCorner.y;   // camera-facing sprites (leaves); zero for normal meshes
    vPos = wp.xyz; vNrm = mat3(uModel) * aNrm; vUV = aUV; vCol = aCol; vUVLM = aUVLM; vUVOV = aUVOV;
    gl_Position = uViewProj * wp;
}
)";

// Lighting follows the original Emmersion pixel shader:
//   Diffuse = (ambient + sun * N.L * afterburnerShadow) * vertexColour
//   Out = texture * Diffuse * afterburnerOcclusion, then fog.
const char* kFS = R"(#version 330 core
in vec3 vPos; in vec3 vNrm; in vec2 vUV; in vec4 vCol; in vec2 vUVLM; in vec2 vUVOV;
uniform sampler2D uTex;
uniform sampler2D uOverlay;
uniform int uHasOV;
uniform sampler2D uLightmap;
uniform int uHasTex;
uniform int uHasLM;
uniform vec3 uCamPos;
uniform float uReflect;
uniform int uMode; // 0 opaque, 1 alpha blend, 2 additive, 3 sky (unlit)
uniform int uIgnoreAlpha;
uniform vec3 uSunDir;
uniform vec3 uSunCol;
uniform vec3 uAmbient;
uniform vec3 uFogCol;
uniform vec3 uFog;       // near, far, max
uniform vec3 uLightData; // occlusion scale, occlusion bias, shadow strength
out vec4 oColor;

vec3 sky(vec3 d) {
    vec3 horizon = vec3(0.86, 0.89, 0.93), zenith = vec3(0.28, 0.50, 0.86), ground = vec3(0.30, 0.28, 0.26);
    return d.y > 0.0 ? mix(horizon, zenith, pow(d.y, 0.55)) : mix(horizon, ground, pow(-d.y, 0.35));
}

void main() {
    vec4 base = (uHasTex != 0) ? texture(uTex, vUV) : vec4(0.55, 0.55, 0.55, 1.0);
    if (uIgnoreAlpha != 0) base.a = 1.0;
    if (uHasOV != 0) { vec4 ov = texture(uOverlay, vUVOV); base.rgb = mix(base.rgb, ov.rgb, ov.a); }
    if (uMode == 2) { oColor = vec4(base.rgb * base.a * vCol.rgb, 1.0); return; }
    if (uMode == 3) { oColor = vec4(base.rgb * vCol.rgb, 1.0); return; }
    if (uMode == 0 && base.a < 0.3) discard;
    vec3 N = normalize(vNrm);
    vec3 V = normalize(uCamPos - vPos);
    if (dot(N, V) < 0.0) N = -N;
    vec3 L = normalize(uSunDir);
    float ndl = max(dot(N, L), 0.0);
    float shadow = 1.0, occ = 1.0;
    if (uHasLM != 0) {
        vec4 lm = texture(uLightmap, vUVLM);
        occ = clamp(1.0 - (lm.g - uLightData.y) * uLightData.x, 0.0, 1.0);
        shadow = lm.a * uLightData.z + (1.0 - uLightData.z);
    }
    vec3 diffuse = (uAmbient + uSunCol * ndl * shadow) * vCol.rgb;
    vec3 col = base.rgb * diffuse * occ;
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 80.0) * ndl * shadow;
    float fres = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 R = reflect(-V, N);
    col += uReflect * sky(R) * (0.06 + 0.55 * fres) * occ + spec * uReflect * 0.6;
    if (uFog.y > uFog.x) {
        float f = clamp((length(uCamPos - vPos) - uFog.x) / (uFog.y - uFog.x), 0.0, 1.0) * uFog.z;
        col = mix(col, uFogCol, f);
    }
    oColor = vec4(col, uMode == 1 ? base.a * vCol.a : 1.0);
}
)";

GLuint compile(GLenum type, const char* src, std::string* err) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; GLsizei n = 0;
        glGetShaderInfoLog(s, sizeof log, &n, log);
        if (err) *err = std::string("shader: ") + std::string(log, n);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

struct Vertex {
    float pos[3];
    float nrm[3];
    float uv[2];
    uint8_t col[4];
    float uvLM[2];
    float uvOV[2];
    float corner[2];
};

GLuint upload_texture(const ep3::Texture& t) {
    if (t.cubemap) return 0; // environment cubemaps handled later
    gcm::DecodedTexture dt;
    if (!gcm::decode(t.format, t.mipmaps, t.cubemap, t.width, t.height, t.texels, dt) || dt.faces.empty() || dt.faces[0].empty())
        return 0;
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    const auto& levels = dt.faces[0];
    for (size_t m = 0; m < levels.size(); ++m) {
        const auto& lv = levels[m];
        if (dt.kind == gcm::PixelKind::RGBA8) {
            glTexImage2D(GL_TEXTURE_2D, GLint(m), GL_RGBA8, GLsizei(lv.width), GLsizei(lv.height), 0, GL_RGBA, GL_UNSIGNED_BYTE, lv.data.data());
        } else {
            GLenum f = dt.kind == gcm::PixelKind::DXT1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
                     : dt.kind == gcm::PixelKind::DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            glCompressedTexImage2D(GL_TEXTURE_2D, GLint(m), f, GLsizei(lv.width), GLsizei(lv.height), 0, GLsizei(lv.data.size()), lv.data.data());
        }
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(levels.size() - 1));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, levels.size() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 16.0f);
    return id;
}

bool texture_has_alpha(const ep3::Texture& t) {
    uint8_t base = t.format & uint8_t(~0x60);
    return base == 0x88 || base == 0x87;
}

bool name_has(const std::string& name, std::initializer_list<const char*> keys) {
    std::string n = name;
    for (auto& c : n) c = char(std::tolower(static_cast<unsigned char>(c)));
    for (auto k : keys) if (n.find(k) != std::string::npos) return true;
    return false;
}

} // namespace

bool Renderer::init(std::string* error) {
    GLuint vs = compile(GL_VERTEX_SHADER, kVS, error);
    if (!vs) return false;
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFS, error);
    if (!fs) return false;
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    glDeleteShader(vs); glDeleteShader(fs);
    if (!ok) {
        char log[2048]; GLsizei n = 0;
        glGetProgramInfoLog(program_, sizeof log, &n, log);
        if (error) *error = std::string("link: ") + std::string(log, n);
        return false;
    }
    uViewProj_ = glGetUniformLocation(program_, "uViewProj");
    uModel_ = glGetUniformLocation(program_, "uModel");
    uCamRight_ = glGetUniformLocation(program_, "uCamRight");
    uCamUp_ = glGetUniformLocation(program_, "uCamUp");
    uCamPos_ = glGetUniformLocation(program_, "uCamPos");
    uTex_ = glGetUniformLocation(program_, "uTex");
    uHasTex_ = glGetUniformLocation(program_, "uHasTex");
    uReflect_ = glGetUniformLocation(program_, "uReflect");
    uMode_ = glGetUniformLocation(program_, "uMode");
    uIgnoreAlpha_ = glGetUniformLocation(program_, "uIgnoreAlpha");
    uLightmap_ = glGetUniformLocation(program_, "uLightmap");
    uHasLM_ = glGetUniformLocation(program_, "uHasLM");
    uSunDir_ = glGetUniformLocation(program_, "uSunDir");
    uSunCol_ = glGetUniformLocation(program_, "uSunCol");
    uAmbient_ = glGetUniformLocation(program_, "uAmbient");
    uFogCol_ = glGetUniformLocation(program_, "uFogCol");
    uFog_ = glGetUniformLocation(program_, "uFog");
    uLightData_ = glGetUniformLocation(program_, "uLightData");
    uOverlay_ = glGetUniformLocation(program_, "uOverlay");
    uHasOV_ = glGetUniformLocation(program_, "uHasOV");
    return true;
}

void Renderer::clearScene() {
    for (auto& d : draws_) {
        glDeleteVertexArrays(1, &d.vao);
        glDeleteBuffers(1, &d.vbo);
        glDeleteBuffers(1, &d.ibo);
    }
    draws_.clear();
    if (!textures_.empty()) glDeleteTextures(GLsizei(textures_.size()), textures_.data());
    textures_.clear();
    triangleCount = 0;
    for (int i = 0; i < 3; ++i) { boundsMin[i] = 1e30f; boundsMax[i] = -1e30f; }
}

void Renderer::shutdown() { clearScene(); }

void Renderer::addPackage(const ep3::Package& pkg, int group) {
    std::map<int, GLuint> tex;
    for (auto& [idx, t] : pkg.textures) {
        GLuint id = upload_texture(t);
        if (id) { tex[idx] = id; textures_.push_back(id); }
    }
    // entity light-state: inherited from ancestors named *_On / *_Off
    std::map<int, const ep3::Entity*> byItem;
    for (auto& e : pkg.entities) byItem[e.item] = &e;
    auto light_state = [&](const ep3::Entity& e) {
        const ep3::Entity* cur = &e;
        for (int guard = 0; cur && guard < 64; ++guard) {
            const std::string& n = cur->name;
            auto ends = [&](const char* s) { size_t l = std::strlen(s); return n.size() >= l && n.compare(n.size() - l, l, s) == 0; };
            if (ends("_On")) return 1;
            if (ends("_Off")) return 2;
            auto it = byItem.find(cur->parent);
            cur = it == byItem.end() ? nullptr : it->second;
        }
        return 0;
    };
    auto is_cube = [&](int t) { auto it = pkg.textures.find(t); return it != pkg.textures.end() && it->second.cubemap; };

    for (auto& e : pkg.entities) {
        const float* M = e.world;
        const int ls = light_state(e);
        for (auto& m : e.meshes) {
            const uint32_t nv = m.vertexCount();
            if (!nv || m.indices.empty()) continue;

            // material layers
            Draw d;
            d.group = group;
            d.entity = e.name;
            int diffuse = -1, lightmap = -1, lmSet = 1, diffSet = 0, blendMap = -1, overlay = -1, ovSet = 0;
            bool hasEnv = false;
            const ep3::Material* mat = nullptr;
            auto mit = pkg.materials.find(m.material);
            if (mit != pkg.materials.end()) {
                mat = &mit->second;
                int uvIdx = -1;
                for (auto& s : mat->slots) {
                    const uint32_t kind = s.type & 0xf, stage = s.type >> 12;
                    (void)stage;
                    // each layer consumes the next UV set, unless flagged 0x80 (reuse previous set)
                    if (!((s.type & 0x80) && uvIdx >= 0)) ++uvIdx;
                    const int set = std::max(0, uvIdx);
                    if (is_cube(s.texture)) { hasEnv = true; continue; }
                    if (kind == 1 && mat->isCarPaint() && stage == 0 && diffuse < 0) diffuse = s.texture;
                    else if (kind == 1 && blendMap < 0) blendMap = s.texture;   // terrain blend amount (TODO: land blending)
                    else if (kind == 2 && diffuse < 0) { diffuse = s.texture; diffSet = set; }
                    else if ((kind == 2 || kind == 4) && overlay < 0) { overlay = s.texture; ovSet = set; }
                    else if (kind == 0xe && lightmap < 0) { lightmap = s.texture; lmSet = set; }
                    else if (kind == 3) hasEnv = true;
                }
                if (diffuse < 0) diffuse = mat->diffuseTexture();
            }
            const auto& lmUV = m.uvSet(lmSet);
            const auto& dUV = m.uvSet(diffSet).size() == size_t(nv) * 2 ? m.uvSet(diffSet) : m.uvs;
            const bool lmOk = lightmap >= 0 && lmUV.size() == size_t(nv) * 2;
            const auto& ovUV = m.uvSet(ovSet);
            const bool ovOk = overlay >= 0 && ovUV.size() == size_t(nv) * 2;

            std::vector<Vertex> vb(nv);
            float c[3]{};
            for (uint32_t v = 0; v < nv; ++v) {
                const float x = m.positions[v * 3], y = m.positions[v * 3 + 1], z = m.positions[v * 3 + 2];
                Vertex& o = vb[v];
                o.pos[0] = x * M[0] + y * M[4] + z * M[8] + M[12];
                o.pos[1] = x * M[1] + y * M[5] + z * M[9] + M[13];
                o.pos[2] = x * M[2] + y * M[6] + z * M[10] + M[14];
                float nx = 0, ny = 1, nz = 0;
                if (!m.normals.empty()) { nx = m.normals[v * 3]; ny = m.normals[v * 3 + 1]; nz = m.normals[v * 3 + 2]; }
                o.nrm[0] = nx * M[0] + ny * M[4] + nz * M[8];
                o.nrm[1] = nx * M[1] + ny * M[5] + nz * M[9];
                o.nrm[2] = nx * M[2] + ny * M[6] + nz * M[10];
                o.uv[0] = dUV.empty() ? 0.f : dUV[v * 2];
                o.uv[1] = dUV.empty() ? 0.f : dUV[v * 2 + 1];
                o.uvLM[0] = lmOk ? lmUV[v * 2] : 0.f;
                o.uvLM[1] = lmOk ? lmUV[v * 2 + 1] : 0.f;
                o.uvOV[0] = ovOk ? ovUV[v * 2] : 0.f;
                o.uvOV[1] = ovOk ? ovUV[v * 2 + 1] : 0.f;
                const bool pc = m.particleCorner.size() == size_t(nv) * 2;
                o.corner[0] = pc ? m.particleCorner[v * 2] : 0.f;
                o.corner[1] = pc ? m.particleCorner[v * 2 + 1] : 0.f;
                if (!m.colors.empty()) std::memcpy(o.col, &m.colors[v * 4], 4);
                else o.col[0] = o.col[1] = o.col[2] = o.col[3] = 255;
                for (int k = 0; k < 3; ++k) {
                    c[k] += o.pos[k];
                    if (ls != 1) { boundsMin[k] = std::min(boundsMin[k], o.pos[k]); boundsMax[k] = std::max(boundsMax[k], o.pos[k]); }
                }
            }
            for (int k = 0; k < 3; ++k) d.center[k] = c[k] / float(nv);
            d.lightState = ls;
            d.count = GLsizei(m.indices.size());
            triangleCount += m.indices.size() / 3;
            if (auto t = tex.find(diffuse); t != tex.end()) d.tex = t->second;
            if (lmOk) if (auto t = tex.find(lightmap); t != tex.end()) d.lightmap = t->second;
            if (ovOk) if (auto t = tex.find(overlay); t != tex.end()) d.overlay = t->second;
            if (mat) {
                auto pt = pkg.textures.find(diffuse);
                const bool alpha = pt != pkg.textures.end() && texture_has_alpha(pt->second);
                // windows with a reflection cubemap store reflectivity in alpha; car paint stores the paint mask
                d.ignoreAlpha = mat->isCarPaint() || hasEnv;
                if ((mat->renderState & 0xff) == 0x45) d.pass = Pass::Additive;
                else if (d.ignoreAlpha || !alpha) d.pass = Pass::Opaque;
                else if (mat->renderState & 1) d.pass = Pass::Blend;   // render state bit 0: alpha blending
                else d.pass = Pass::Opaque;                           // alpha-tested cutout (foliage, fences)
                if (hasEnv && (mat->isCarPaint() || !lmOk)) d.reflect = 1.0f;
            }

            glGenVertexArrays(1, &d.vao);
            glBindVertexArray(d.vao);
            glGenBuffers(1, &d.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
            glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vb.size() * sizeof(Vertex)), vb.data(), GL_STATIC_DRAW);
            glGenBuffers(1, &d.ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(m.indices.size() * 4), m.indices.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, nrm));
            glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, uv));
            glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, 1, sizeof(Vertex), (void*)offsetof(Vertex, col));
            glEnableVertexAttribArray(4); glVertexAttribPointer(4, 2, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, uvLM));
            glEnableVertexAttribArray(5); glVertexAttribPointer(5, 2, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, uvOV));
            glEnableVertexAttribArray(6); glVertexAttribPointer(6, 2, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, corner));
            glBindVertexArray(0);
            draws_.push_back(d);
        }
    }
}

void Renderer::draw(const float* viewProj, const float camPos[3], int width, int height, const DrawOptions& opt) {
    glViewport(0, 0, width, height);
    glClearColor(env.skyColour[0], env.skyColour[1], env.skyColour[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, opt.wireframe ? GL_LINE : GL_FILL);
    glUseProgram(program_);
    glUniformMatrix4fv(uViewProj_, 1, 0, viewProj);
    glUniform3f(uCamPos_, camPos[0], camPos[1], camPos[2]);
    glUniform3f(uCamRight_, camRight[0], camRight[1], camRight[2]);
    glUniform3f(uCamUp_, camUp[0], camUp[1], camUp[2]);
    glUniform1i(uTex_, 0);
    glUniform1i(uLightmap_, 1);
    glUniform1i(uOverlay_, 2);
    glUniform3f(uSunDir_, env.sunDir[0], env.sunDir[1], env.sunDir[2]);
    glUniform3f(uSunCol_, env.sunColour[0], env.sunColour[1], env.sunColour[2]);
    glUniform3f(uAmbient_, env.ambient[0], env.ambient[1], env.ambient[2]);
    glUniform3f(uFogCol_, env.fogColour[0], env.fogColour[1], env.fogColour[2]);
    glUniform3f(uFog_, env.fogNear, env.fogFar, env.fogMax);
    glUniform3f(uLightData_, env.lightData[0], env.lightData[1], env.lightData[2]);

    auto visible = [&](const Draw& d) { return !d.hidden && d.group != hiddenGroup && (d.lightState == 0 || (d.lightState == 1) == opt.lightsOn); };
    auto issue = [&](const Draw& d, int mode) {
        glUniform1i(uHasTex_, d.tex ? 1 : 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, d.lightmap);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, d.overlay);
        glUniform1i(uHasOV_, d.overlay ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, d.tex);
        glUniform1i(uHasLM_, (d.lightmap && opt.lightmaps) ? 1 : 0);
        glUniform1f(uReflect_, d.reflect);
        glUniform1i(uMode_, mode);
        glUniform1i(uIgnoreAlpha_, d.ignoreAlpha ? 1 : 0);
        glUniformMatrix4fv(uModel_, 1, 0, d.model);
        glBindVertexArray(d.vao);
        glDrawElements(GL_TRIANGLES, d.count, GL_UNSIGNED_INT, nullptr);
    };

    // sky dome first: follows the camera, no depth
    glDisable(GL_BLEND);
    glDepthMask(0);
    glDisable(GL_DEPTH_TEST);
    for (auto& d : draws_) {
        if (!d.sky || !visible(d)) continue;
        float m[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, camPos[0], camPos[1] - 25.0f, camPos[2], 1};
        std::memcpy(d.model, m, sizeof m);
        issue(d, 3);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthMask(1);
    for (auto& d : draws_) if (!d.sky && d.pass == Pass::Opaque && visible(d)) issue(d, 0);

    // transparent: back to front
    std::vector<const Draw*> tr;
    for (auto& d : draws_) if (!d.sky && d.pass != Pass::Opaque && visible(d)) tr.push_back(&d);
    auto dist2 = [&](const Draw* d) { float s = 0; for (int k = 0; k < 3; ++k) { float t = d->center[k] + d->model[12 + k] - camPos[k]; s += t * t; } return s; };
    std::sort(tr.begin(), tr.end(), [&](const Draw* a, const Draw* b) { return dist2(a) > dist2(b); });
    glEnable(GL_BLEND);
    glDepthMask(0);
    for (auto* d : tr) {
        if (d->pass == Pass::Additive) { glBlendFunc(GL_ONE, GL_ONE); issue(*d, 2); }
        else { glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); issue(*d, 1); }
    }
    glDepthMask(1);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

} // namespace gti::render

namespace gti::render {
void Renderer::setGroupTransform(int group, const float* m) {
    for (auto& d : draws_) if (d.group == group) std::memcpy(d.model, m, sizeof d.model);
}
void Renderer::setEntityTransform(int group, const std::string& entity, const float* m) {
    for (auto& d : draws_) if (d.group == group && d.entity == entity) std::memcpy(d.model, m, sizeof d.model);
}
void Renderer::setEntityHidden(int group, const std::string& entity, bool hidden) {
    for (auto& d : draws_) if (d.group == group && d.entity == entity) d.hidden = hidden;
}
void Renderer::removeGroup(int group) {
    for (auto it = draws_.begin(); it != draws_.end();) {
        if (it->group == group) {
            glDeleteVertexArrays(1, &it->vao); glDeleteBuffers(1, &it->vbo); glDeleteBuffers(1, &it->ibo);
            it = draws_.erase(it);
        } else ++it;
    }
}
} // namespace gti::render

namespace gti::render {
void Renderer::setGroupSky(int group) {
    for (auto& d : draws_) if (d.group == group) d.sky = true;
}
void Renderer::setGroupReflective(int group, float amount) {
    for (auto& d : draws_) if (d.group == group) d.reflect = amount;
}
} // namespace gti::render
