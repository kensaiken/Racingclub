p='src/render/renderer.cpp'; s=open(p,encoding='utf-8').read()
def rep(a,b):
    global s
    assert a in s, a[:70]; s=s.replace(a,b,1)
rep("uniform int uMode; // 0 opaque, 1 alpha blend, 2 additive","uniform int uMode; // 0 opaque, 1 alpha blend, 2 additive, 3 sky (unlit)")
rep("    if (uMode == 2) { oColor = vec4(base.rgb * base.a * vCol.rgb, 1.0); return; }",
    "    if (uMode == 2) { oColor = vec4(base.rgb * base.a * vCol.rgb, 1.0); return; }\n    if (uMode == 3) { oColor = vec4(base.rgb * vCol.rgb, 1.0); return; }")
rep("""    glDisable(GL_BLEND);
    glDepthMask(1);
    for (auto& d : draws_) if (d.pass == Pass::Opaque && visible(d)) issue(d, 0);""",
"""    // sky dome first: follows the camera, no depth
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
    for (auto& d : draws_) if (!d.sky && d.pass == Pass::Opaque && visible(d)) issue(d, 0);""")
rep("""    for (auto& d : draws_) if (d.pass != Pass::Opaque && visible(d)) tr.push_back(&d);""",
    """    for (auto& d : draws_) if (!d.sky && d.pass != Pass::Opaque && visible(d)) tr.push_back(&d);""")
s += """
namespace gti::render {
void Renderer::setGroupSky(int group) {
    for (auto& d : draws_) if (d.group == group) d.sky = true;
}
void Renderer::setGroupReflective(int group, float amount) {
    for (auto& d : draws_) if (d.group == group) d.reflect = amount;
}
} // namespace gti::render
"""
open(p,'w',encoding='utf-8').write(s)
p='src/render/renderer.h'; s=open(p,encoding='utf-8').read()
s=s.replace("    void removeGroup(int group);","    void removeGroup(int group);\n    void setGroupSky(int group);                 // unlit dome drawn first, centred on the camera\n    void setGroupReflective(int group, float amount);")
s=s.replace("        bool hidden = false;","        bool hidden = false;\n        bool sky = false;")
open(p,'w',encoding='utf-8').write(s)
p='src/main.cpp'; s=open(p,encoding='utf-8').read()
a="""        // geometry here is HUD map quads + the animated train: only the lights are used for now"""
b="""        // geometry here is HUD map quads + the animated train: only the lights are used for now
    }
    {
        const fs::path tso = dir.parent_path().parent_path() / "tracksideobjects";
        ep3::Package sky, sea;
        if (sky.load((tso / "sky" / "sky.ep3").string())) { r.addPackage(sky, 2); r.setGroupSky(2); }
        if (sea.load((tso / "sea" / "sea.ep3").string())) { r.addPackage(sea, 3); r.setGroupReflective(3, 1.0f); }"""
assert a in s; s=s.replace(a,b,1)
open(p,'w',encoding='utf-8').write(s)
print('ok')
