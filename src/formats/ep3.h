#pragma once
// GTI Club (Sumo Digital "Emmersion" engine, PS3) .ep3 package reader.
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gti::ep3 {

struct Item {
    int index = 0;
    uint32_t off = 0, size = 0;
    std::string tag;            // e.g. "EMTH" texture, "EMDH" material, "EEDH" entity, "ECEH" camera, "EODH" locator
    uint32_t data = 0;          // absolute offset of the referenced data block (0 = none)
    int parent = -1;            // parent item index (entities)
    std::string path;           // full source path / uid string
    std::string tagName;        // short user tag (e.g. "WheelRL", "HeadLightOn")
};

struct Texture {
    std::string name;
    uint8_t format = 0, mipmaps = 1, dimension = 2;
    bool cubemap = false;
    uint16_t width = 0, height = 0, depth = 1;
    std::vector<uint8_t> texels;
};

struct MaterialSlot { uint32_t type = 0; int texture = -1; };

struct Material {
    std::string name;
    uint32_t shader = 0;        // +0x50
    uint32_t renderState = 0;   // +0x54
    std::vector<MaterialSlot> slots;
    bool isCarPaint() const { return (shader & 0xfff) == 0x203; }
    int diffuseTexture() const {
        // stage 0 colour map: low nibble 2 = diffuse, 1 = car-paint base (alpha = paint mask)
        for (auto& s : slots) if (((s.type & 0xf) == 2 || (s.type & 0xf) == 1) && (s.type >> 12) == 0) return s.texture;
        for (auto& s : slots) if ((s.type & 0xf) == 2) return s.texture;
        return -1;
    }
};

struct MeshSegment {
    int material = -1;
    std::vector<float> positions;   // xyz
    std::vector<float> normals;     // xyz (may be empty)
    std::vector<float> uvs;         // uv set 0 (may be empty)
    std::vector<float> uvsExtra[3]; // uv sets 1..3 (lightmap / extra layers)
    const std::vector<float>& uvSet(int i) const { return i <= 0 ? uvs : uvsExtra[std::min(i, 3) - 1]; }
    std::vector<uint8_t> colors;    // rgba (may be empty)
    std::vector<float> particleCorner; // xy view-space corner offsets of camera-facing sprites (attr 17), may be empty
    std::vector<uint32_t> indices;
    uint32_t vertexCount() const { return uint32_t(positions.size() / 3); }
};

struct Entity {
    int item = -1;
    int parent = -1;            // item index of parent entity
    std::string name;           // short name (last path component)
    float local[16]{};          // row-major, row-vector convention (translation in row 3)
    float world[16]{};
    std::vector<MeshSegment> meshes;
};

struct Light {
    int item = -1;
    uint32_t flags = 0;         // +0x54
    float matrix[16]{};         // +0x60 rows (row 2 = direction for directional lights, row 3 = position)
    float colour[4]{};          // +0xa0
    float range = 0.f;          // +0xb4
    bool directional() const { return matrix[12] == 0.f && matrix[13] == 0.f && matrix[14] == 0.f; }
};

struct Camera {                 // ECEH: e.g. the trackside replay cameras (replaycameras.ep3)
    std::string name;
    float pos[3]{};
    float fovDeg = 30;
};

struct Locator {
    int item = -1, parent = -1;
    std::string name;           // e.g. "hardPointFL", "Start_Lower_0"
    std::string tag;            // user tag, e.g. "HardFL"
    float matrix[16]{};         // row-major, translation in row 3
};

class Package {
public:
    bool load(const std::string& path, std::string* error = nullptr);

    std::string path;
    std::vector<Item> items;
    std::map<int, Texture> textures;    // by item index
    std::map<int, Material> materials;  // by item index
    std::vector<Entity> entities;
    std::vector<Light> lights;
    std::vector<Locator> locators;
    std::vector<Camera> cameras;
    const Locator* findLocator(const std::string& nameOrTag) const {
        for (auto& l : locators) if (l.name == nameOrTag || l.tag == nameOrTag) return &l;
        return nullptr;
    }
    std::vector<std::string> warnings;

private:
    std::vector<uint8_t> d_;
    bool parseItems();
    void parseTexture(const Item& it);
    void parseMaterial(const Item& it);
    void parseEntity(const Item& it);
};

} // namespace gti::ep3
