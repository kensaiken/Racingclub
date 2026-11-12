#pragma once
// Static world collision built from the track render mesh (uniform XZ grid of triangles).
#include "formats/ep3.h"
#include "game/vmath.h"
#include <cstdint>
#include <vector>

namespace gti {

struct RayHit {
    float t = 0;      // distance along the (normalized) ray
    V3 point, normal;
};

class CollisionWorld {
public:
    // Adds the opaque geometry of a package (foliage / decals / glass are skipped).
    void addPackage(const ep3::Package& pkg);
    void addTriangle(const V3& a, const V3& b, const V3& c);
    void build(float cellSize = 4.0f);
    bool raycast(const V3& origin, const V3& dir, float maxDist, RayHit& hit) const;
    size_t triangleCount() const { return tris_.size(); }

private:
    struct Tri { V3 a, b, c, n; };
    std::vector<Tri> tris_;
    float cell_ = 4.f, minX_ = 0, minZ_ = 0;
    int nx_ = 0, nz_ = 0;
    std::vector<uint32_t> cellStart_, cellItems_;
};

} // namespace gti
