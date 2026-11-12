def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)

patch('src/game/collision.h', [("    void build(float cellSize = 4.0f);", "    void addTriangle(const V3& a, const V3& b, const V3& c);\n    void build(float cellSize = 4.0f);")])
patch('src/game/collision.cpp', [("void CollisionWorld::build(float cellSize) {", """void CollisionWorld::addTriangle(const V3& a, const V3& b, const V3& c) {
    Tri t{a, b, c, {}};
    V3 n = cross(b - a, c - a);
    float l = length(n);
    if (l < 1e-8f) return;
    t.n = n * (1.0f / l);
    tris_.push_back(t);
}

void CollisionWorld::build(float cellSize) {""")])
patch('src/game/vehicle.h', [("    CarParams params;\n", "    CarParams params;\n    float steerAngle() const { return steerState_; }\n")])
patch('CMakeLists.txt', [("# CLI tool to dump", """# headless physics test bench
add_executable(physlab src/tools/physlab.cpp src/game/collision.cpp src/game/vehicle.cpp)
target_link_libraries(physlab PRIVATE gti_formats)

# CLI tool to dump"""), ("foreach(t gtiport ep3tool)", "foreach(t gtiport ep3tool physlab)")])
print('ok')
