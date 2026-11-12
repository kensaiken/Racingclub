#pragma once
// Small vector / quaternion helpers for gameplay code.
#include <cmath>

namespace gti {

struct V3 {
    float x = 0, y = 0, z = 0;
    V3() = default;
    constexpr V3(float a, float b, float c) : x(a), y(b), z(c) {}
    V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator*(float s) const { return {x * s, y * s, z * s}; }
    V3 operator-() const { return {-x, -y, -z}; }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};
inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(const V3& a, const V3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(const V3& a) { return std::sqrt(dot(a, a)); }
inline V3 normalize(const V3& a) { float l = length(a); return l > 1e-9f ? a * (1.0f / l) : V3{0, 0, 0}; }

struct Quat {
    float w = 1, x = 0, y = 0, z = 0;
    static Quat axisAngle(const V3& ax, float ang) {
        V3 a = gti::normalize(ax);
        float s = std::sin(ang * 0.5f);
        return {std::cos(ang * 0.5f), a.x * s, a.y * s, a.z * s};
    }
    Quat operator*(const Quat& q) const {
        return {w * q.w - x * q.x - y * q.y - z * q.z, w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x, w * q.z + x * q.y - y * q.x + z * q.w};
    }
    V3 rotate(const V3& v) const {
        V3 u{x, y, z};
        V3 t = cross(u, v) * 2.0f;
        return v + t * w + cross(u, t);
    }
    V3 unrotate(const V3& v) const { return Quat{w, -x, -y, -z}.rotate(v); }
    void normalize() {
        float l = std::sqrt(w * w + x * x + y * y + z * z);
        w /= l; x /= l; y /= l; z /= l;
    }
};

// Column-major 4x4 (OpenGL convention) built from rotation + translation.
inline void to_matrix(const Quat& q, const V3& t, float* m) {
    V3 cx = q.rotate({1, 0, 0}), cy = q.rotate({0, 1, 0}), cz = q.rotate({0, 0, 1});
    m[0] = cx.x; m[1] = cx.y; m[2] = cx.z; m[3] = 0;
    m[4] = cy.x; m[5] = cy.y; m[6] = cy.z; m[7] = 0;
    m[8] = cz.x; m[9] = cz.y; m[10] = cz.z; m[11] = 0;
    m[12] = t.x; m[13] = t.y; m[14] = t.z; m[15] = 1;
}
inline void mat_mul(const float* a, const float* b, float* o) { // o = a*b column-major
    float r[16];
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[k * 4 + rr] * b[c * 4 + k];
            r[c * 4 + rr] = s;
        }
    for (int i = 0; i < 16; ++i) o[i] = r[i];
}

} // namespace gti
