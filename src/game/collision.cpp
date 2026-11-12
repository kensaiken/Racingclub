#include "game/collision.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace gti {

namespace {
bool has_alpha(const ep3::Package& pkg, int tex) {
    auto it = pkg.textures.find(tex);
    if (it == pkg.textures.end()) return false;
    uint8_t b = it->second.format & uint8_t(~0x60);
    return b == 0x88 || b == 0x87;
}
} // namespace

void CollisionWorld::addPackage(const ep3::Package& pkg) {
    for (auto& e : pkg.entities) {
        const float* M = e.world;
        for (auto& m : e.meshes) {
            // skip see-through layers: alpha-tested foliage, blended decals / glass, additive flares
            if (!m.particleCorner.empty()) continue;   // camera-facing leaf sprites
            bool groundOnly = false;                    // alpha-tested layer: keep only its walkable (upward) faces
            auto mit = pkg.materials.find(m.material);
            if (mit != pkg.materials.end()) {
                const auto& mat = mit->second;
                bool cube = false;
                for (auto& s : mat.slots) { auto t = pkg.textures.find(s.texture); if (t != pkg.textures.end() && t->second.cubemap) cube = true; }
                int diffuse = -1;
                for (auto& s : mat.slots) if ((s.type & 0xf) == 2) { auto t = pkg.textures.find(s.texture); if (t != pkg.textures.end() && !t->second.cubemap) { diffuse = s.texture; break; } }
                if ((mat.renderState & 0xff) == 0x45) continue;
                // grass verges and ground decals use DXT5 too: they must stay solid, foliage cards must not
                if (!cube && has_alpha(pkg, diffuse)) groundOnly = true;
            }
            const uint32_t nv = m.vertexCount();
            std::vector<V3> w(nv);
            for (uint32_t v = 0; v < nv; ++v) {
                float x = m.positions[v * 3], y = m.positions[v * 3 + 1], z = m.positions[v * 3 + 2];
                w[v] = {x * M[0] + y * M[4] + z * M[8] + M[12], x * M[1] + y * M[5] + z * M[9] + M[13], x * M[2] + y * M[6] + z * M[10] + M[14]};
            }
            for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
                Tri t{w[m.indices[i]], w[m.indices[i + 1]], w[m.indices[i + 2]], {}};
                V3 n = cross(t.b - t.a, t.c - t.a);
                float l = length(n);
                if (l < 1e-8f) continue;
                t.n = n * (1.0f / l);
                if (groundOnly && std::fabs(t.n.y) < 0.7f) continue;
                tris_.push_back(t);
            }
        }
    }
}

void CollisionWorld::addTriangle(const V3& a, const V3& b, const V3& c) {
    Tri t{a, b, c, {}};
    V3 n = cross(b - a, c - a);
    float l = length(n);
    if (l < 1e-8f) return;
    t.n = n * (1.0f / l);
    tris_.push_back(t);
}

void CollisionWorld::build(float cellSize) {
    cell_ = cellSize;
    float maxX = -1e30f, maxZ = -1e30f;
    minX_ = minZ_ = 1e30f;
    for (auto& t : tris_)
        for (const V3* p : {&t.a, &t.b, &t.c}) {
            minX_ = std::min(minX_, p->x); maxX = std::max(maxX, p->x);
            minZ_ = std::min(minZ_, p->z); maxZ = std::max(maxZ, p->z);
        }
    if (tris_.empty()) { nx_ = nz_ = 0; return; }
    nx_ = int((maxX - minX_) / cell_) + 1;
    nz_ = int((maxZ - minZ_) / cell_) + 1;
    std::vector<uint32_t> counts(size_t(nx_) * nz_ + 1, 0);
    auto range = [&](const Tri& t, int& x0, int& x1, int& z0, int& z1) {
        float a = std::min({t.a.x, t.b.x, t.c.x}), b = std::max({t.a.x, t.b.x, t.c.x});
        float c = std::min({t.a.z, t.b.z, t.c.z}), d = std::max({t.a.z, t.b.z, t.c.z});
        x0 = std::clamp(int((a - minX_) / cell_), 0, nx_ - 1); x1 = std::clamp(int((b - minX_) / cell_), 0, nx_ - 1);
        z0 = std::clamp(int((c - minZ_) / cell_), 0, nz_ - 1); z1 = std::clamp(int((d - minZ_) / cell_), 0, nz_ - 1);
    };
    for (auto& t : tris_) {
        int x0, x1, z0, z1; range(t, x0, x1, z0, z1);
        for (int z = z0; z <= z1; ++z) for (int x = x0; x <= x1; ++x) counts[size_t(z) * nx_ + x]++;
    }
    cellStart_.assign(counts.size(), 0);
    for (size_t i = 1; i < counts.size(); ++i) cellStart_[i] = cellStart_[i - 1] + counts[i - 1];
    cellItems_.assign(cellStart_.back() + counts.back(), 0);
    std::vector<uint32_t> fill(cellStart_.begin(), cellStart_.end());
    for (uint32_t i = 0; i < tris_.size(); ++i) {
        int x0, x1, z0, z1; range(tris_[i], x0, x1, z0, z1);
        for (int z = z0; z <= z1; ++z) for (int x = x0; x <= x1; ++x) cellItems_[fill[size_t(z) * nx_ + x]++] = i;
    }
}

bool CollisionWorld::raycast(const V3& o, const V3& dirIn, float maxDist, RayHit& hit) const {
    if (!nx_) return false;
    V3 d = normalize(dirIn);
    V3 e = o + d * maxDist;
    int x0 = std::clamp(int((std::min(o.x, e.x) - minX_) / cell_), 0, nx_ - 1), x1 = std::clamp(int((std::max(o.x, e.x) - minX_) / cell_), 0, nx_ - 1);
    int z0 = std::clamp(int((std::min(o.z, e.z) - minZ_) / cell_), 0, nz_ - 1), z1 = std::clamp(int((std::max(o.z, e.z) - minZ_) / cell_), 0, nz_ - 1);
    if ((x1 - x0 + 1) * (z1 - z0 + 1) > 4096) return false; // rays are meant to be short
    bool found = false;
    float best = maxDist;
    std::set<uint32_t> seen;
    const bool multi = (x1 > x0) || (z1 > z0);
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) {
            size_t c = size_t(z) * nx_ + x;
            for (uint32_t k = cellStart_[c]; k < cellStart_[c + 1]; ++k) {
                uint32_t ti = cellItems_[k];
                if (multi && !seen.insert(ti).second) continue;
                const Tri& t = tris_[ti];
                // Moller-Trumbore (double sided)
                V3 e1 = t.b - t.a, e2 = t.c - t.a, p = cross(d, e2);
                float det = dot(e1, p);
                if (std::fabs(det) < 1e-9f) continue;
                float inv = 1.0f / det;
                V3 s = o - t.a;
                float u = dot(s, p) * inv;
                if (u < 0 || u > 1) continue;
                V3 q = cross(s, e1);
                float v = dot(d, q) * inv;
                if (v < 0 || u + v > 1) continue;
                float tt = dot(e2, q) * inv;
                if (tt < 0 || tt >= best) continue;
                best = tt; found = true;
                hit.t = tt; hit.point = o + d * tt;
                hit.normal = dot(t.n, d) > 0 ? -t.n : t.n;
            }
        }
    return found;
}

} // namespace gti
