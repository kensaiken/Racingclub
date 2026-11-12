#pragma once
#include "formats/ep3.h"
#include "render/gl_loader.h"
#include <string>
#include <vector>

namespace gti::render {

struct DrawOptions {
    bool wireframe = false;
    bool lightsOn = false;
    bool lightmaps = true;
};

// Scene lighting (mirrors the original Emmersion shader inputs).
struct Environment {
    float sunDir[3]{0.45f, 0.85f, 0.30f};   // towards the sun
    float sunColour[3]{1.0f, 0.96f, 0.88f};
    float ambient[3]{0.40f, 0.42f, 0.46f};
    float skyColour[3]{0.42f, 0.55f, 0.72f};
    float fogColour[3]{0.6f, 0.74f, 0.91f};
    float fogNear = 0.f, fogFar = 0.f, fogMax = 0.f;   // fogFar <= fogNear disables fog
    float lightData[3]{1.0f, 0.0f, 1.0f};              // AfterBurner occlusion scale/bias, shadow strength
};

class Renderer {
public:
    bool init(std::string* error);
    void shutdown();

    void clearScene();
    void addPackage(const ep3::Package& pkg, int group = 0);
    // dynamic objects: per-draw model matrix (column-major), addressed by group / entity name
    void setGroupTransform(int group, const float* m);
    void setEntityTransform(int group, const std::string& entity, const float* m);
    void setEntityHidden(int group, const std::string& entity, bool hidden);
    void removeGroup(int group);
    int hiddenGroup = -1;                        // group skipped while drawing (cockpit camera hides the player car)
    void setGroupSky(int group);                 // unlit dome drawn first, centred on the camera
    void setGroupReflective(int group, float amount);
    void setPackage(const ep3::Package& pkg) { clearScene(); addPackage(pkg, 0); }

    Environment env;
    float camRight[3]{1, 0, 0}, camUp[3]{0, 1, 0};   // camera basis for camera-facing sprites

    // viewProj: column-major 4x4
    void draw(const float* viewProj, const float camPos[3], int width, int height, const DrawOptions& opt);

    float boundsMin[3]{1e30f, 1e30f, 1e30f}, boundsMax[3]{-1e30f, -1e30f, -1e30f};
    size_t triangleCount = 0;

private:
    enum class Pass { Opaque, Blend, Additive };
    struct Draw {
        GLuint vao = 0, vbo = 0, ibo = 0;
        GLsizei count = 0;
        GLuint tex = 0, lightmap = 0, overlay = 0;
        Pass pass = Pass::Opaque;
        float reflect = 0.f;
        bool ignoreAlpha = false;
        float center[3]{};
        int lightState = 0; // 0 = always, 1 = only lights on, 2 = only lights off
        int group = 0;
        bool hidden = false;
        bool sky = false;
        std::string entity;
        float model[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    };
    GLuint program_ = 0;
    GLint uViewProj_ = -1, uModel_ = -1, uCamRight_ = -1, uCamUp_ = -1, uCamPos_ = -1, uTex_ = -1, uHasTex_ = -1, uReflect_ = -1, uMode_ = -1, uIgnoreAlpha_ = -1;
    GLint uLightmap_ = -1, uHasLM_ = -1, uSunDir_ = -1, uSunCol_ = -1, uAmbient_ = -1, uFogCol_ = -1, uFog_ = -1, uLightData_ = -1, uOverlay_ = -1, uHasOV_ = -1;
    std::vector<Draw> draws_;
    std::vector<GLuint> textures_;
};

} // namespace gti::render
