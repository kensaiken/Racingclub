def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)

patch('src/formats/ep3.h', [("    std::vector<uint8_t> colors;    // rgba (may be empty)",
  "    std::vector<uint8_t> colors;    // rgba (may be empty)\n    std::vector<float> particleCorner; // xy view-space corner offsets of camera-facing sprites (attr 17), may be empty")])
patch('src/formats/ep3.cpp', [("                            } else if ((aid == 4 || aid == 9) && afmt == 4 && acnt == 4) {",
  """                            } else if (aid == 17 && afmt == 2 && acnt >= 2) {
                                ms.particleCorner.resize(size_t(nv) * 2);
                                for (uint32_t i = 0; i < nv; ++i) {
                                    ms.particleCorner[i * 2] = befloat(vb + i * stride + aoff);
                                    ms.particleCorner[i * 2 + 1] = befloat(vb + i * stride + aoff + 4);
                                }
                            } else if ((aid == 4 || aid == 9) && afmt == 4 && acnt == 4) {""")])
patch('src/render/renderer.cpp', [
 ("layout(location=5) in vec2 aUVOV;", "layout(location=5) in vec2 aUVOV;\nlayout(location=6) in vec2 aCorner;\nuniform vec3 uCamRight;\nuniform vec3 uCamUp;"),
 ("    vec4 wp = uModel * vec4(aPos, 1.0);", "    vec4 wp = uModel * vec4(aPos, 1.0);\n    wp.xyz += uCamRight * aCorner.x + uCamUp * aCorner.y;   // camera-facing sprites (leaves); zero for normal meshes"),
 ("    float uvOV[2];\n};", "    float uvOV[2];\n    float corner[2];\n};"),
 ("                o.uvOV[1] = ovOk ? ovUV[v * 2 + 1] : 0.f;", "                o.uvOV[1] = ovOk ? ovUV[v * 2 + 1] : 0.f;\n                const bool pc = m.particleCorner.size() == size_t(nv) * 2;\n                o.corner[0] = pc ? m.particleCorner[v * 2] : 0.f;\n                o.corner[1] = pc ? m.particleCorner[v * 2 + 1] : 0.f;"),
 ("            glEnableVertexAttribArray(5); glVertexAttribPointer(5, 2, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, uvOV));",
  "            glEnableVertexAttribArray(5); glVertexAttribPointer(5, 2, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, uvOV));\n            glEnableVertexAttribArray(6); glVertexAttribPointer(6, 2, GL_FLOAT, 0, sizeof(Vertex), (void*)offsetof(Vertex, corner));"),
 ('    uModel_ = glGetUniformLocation(program_, "uModel");', '    uModel_ = glGetUniformLocation(program_, "uModel");\n    uCamRight_ = glGetUniformLocation(program_, "uCamRight");\n    uCamUp_ = glGetUniformLocation(program_, "uCamUp");'),
 ("    glUniform3f(uCamPos_, camPos[0], camPos[1], camPos[2]);", "    glUniform3f(uCamPos_, camPos[0], camPos[1], camPos[2]);\n    glUniform3f(uCamRight_, camRight[0], camRight[1], camRight[2]);\n    glUniform3f(uCamUp_, camUp[0], camUp[1], camUp[2]);"),
])
patch('src/render/renderer.h', [("GLint uViewProj_ = -1, uModel_ = -1,", "GLint uViewProj_ = -1, uModel_ = -1, uCamRight_ = -1, uCamUp_ = -1,"),
 ("    Environment env;", "    Environment env;\n    float camRight[3]{1, 0, 0}, camUp[3]{0, 1, 0};   // camera basis for camera-facing sprites")])
patch('src/main.cpp', [("        Mat4 view = look_at(eye, look, up);", """        Mat4 view = look_at(eye, look, up);
        renderer.camRight[0] = view.m[0]; renderer.camRight[1] = view.m[4]; renderer.camRight[2] = view.m[8];
        renderer.camUp[0] = view.m[1]; renderer.camUp[1] = view.m[5]; renderer.camUp[2] = view.m[9];""")])
print('ok')
