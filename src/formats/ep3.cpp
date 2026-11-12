#include "formats/ep3.h"
#include "formats/edge_indexes.h"
#include "util/bytes.h"

#include <zlib.h>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <set>

namespace gti {

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? size_t(n) : 0);
    size_t got = n > 0 ? std::fread(out.data(), 1, out.size(), f) : 0;
    std::fclose(f);
    return got == out.size();
}

namespace ep3 {
namespace {

std::string tag_at(const std::vector<uint8_t>& d, size_t o) {
    if (o + 8 > d.size()) return {};
    return std::string{char(d[o + 7]), char(d[o + 6]), char(d[o + 5]), char(d[o + 4])};
}
bool is_tag(const std::string& t) {
    if (t.size() != 4) return false;
    for (char c : t) if (c < 'A' || c > 'Z') return false;
    return true;
}

std::string clean(std::string s) {
    while (s.rfind("pad.", 0) == 0) s.erase(0, 4);
    size_t z = s.find("Z:");
    if (z != std::string::npos) return s.substr(z);
    static const char* align = "ALIGN";
    for (int n = 5; n >= 1; --n)
        if (s.size() > size_t(n) && s.compare(0, n, align, n) == 0) return s.substr(n);
    return s;
}

std::vector<std::string> strings_in(const std::vector<uint8_t>& d, size_t b, size_t e) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = b; i < e && i < d.size(); ++i) {
        uint8_t c = d[i];
        if (c >= 0x20 && c < 0x7f) cur.push_back(char(c));
        else { if (cur.size() >= 3) out.push_back(cur); cur.clear(); }
    }
    if (cur.size() >= 3) out.push_back(cur);
    return out;
}

struct Block {
    const std::vector<uint8_t>* d = nullptr;
    size_t base = 0;
    std::map<uint32_t, uint32_t> fx;
    std::vector<uint8_t> stream;
    bool ok = false;

    bool in(size_t rel, size_t n) const { return base + rel + n <= d->size(); }
    uint32_t u(size_t rel) const { return in(rel, 4) ? be32(d->data() + base + rel) : 0; }
    const uint8_t* p(size_t rel) const { return d->data() + base + rel; }
    int64_t ptr(uint32_t rel) const { auto it = fx.find(rel); return it == fx.end() ? -1 : int64_t(it->second); }
};

bool inflate_raw(const uint8_t* src, size_t n, std::vector<uint8_t>& out, size_t expect) {
    z_stream s{};
    if (inflateInit2(&s, -15) != Z_OK) return false;
    size_t start = out.size();
    out.resize(start + expect);
    s.next_in = const_cast<Bytef*>(src); s.avail_in = uInt(n);
    s.next_out = out.data() + start; s.avail_out = uInt(expect);
    int r = inflate(&s, Z_FINISH);
    size_t produced = expect - s.avail_out;
    inflateEnd(&s);
    out.resize(start + produced);
    return r == Z_STREAM_END || r == Z_OK || r == Z_BUF_ERROR;
}

Block load_block(const std::vector<uint8_t>& d, size_t base, std::string* warn) {
    Block b; b.d = &d; b.base = base;
    if (base + 0x40 > d.size()) return b;
    uint32_t nfx = b.u(0x38), fxoff = b.u(0x3c);
    if (nfx > 100000 || !b.in(fxoff, size_t(nfx) * 12)) return b;
    for (uint32_t i = 0; i < nfx; ++i) {
        const uint8_t* e = b.p(fxoff + i * 12);
        b.fx[be32(e + 4)] = be32(e + 8);
    }
    int64_t sd = b.ptr(0x48);
    if (sd >= 0 && b.in(size_t(sd), 12)) {
        uint32_t absOff = b.u(size_t(sd)), raw = b.u(size_t(sd) + 4), nch = b.u(size_t(sd) + 8);
        if (absOff && raw && nch < 4096 && b.in(size_t(sd) + 12, nch * 4)) {
            size_t pos = absOff;
            b.stream.reserve(raw);
            for (uint32_t c = 0; c < nch; ++c) {
                uint32_t cs = b.u(size_t(sd) + 12 + c * 4);
                if (pos + cs > d.size()) break;
                if (!inflate_raw(d.data() + pos, cs, b.stream, raw - b.stream.size())) break;
                pos += cs;
            }
            if (b.stream.size() != raw && warn) *warn = "stream size mismatch";
        }
    }
    b.ok = true;
    return b;
}

void decode_x11y11z10n(uint32_t w, float* n) {
    int x = int(w >> 21), y = int((w >> 10) & 0x7ff), z = int(w & 0x3ff);
    if (x & 0x400) x -= 0x800;
    if (y & 0x400) y -= 0x800;
    if (z & 0x200) z -= 0x400;
    n[0] = x / 1023.0f; n[1] = y / 1023.0f; n[2] = z / 511.0f;
}

void matmul(const float* a, const float* b, float* o) { // o = a * b (row-major)
    float r[16];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[i * 4 + k] * b[k * 4 + j];
            r[i * 4 + j] = s;
        }
    std::memcpy(o, r, sizeof r);
}

} // namespace

bool Package::load(const std::string& p, std::string* error) {
    path = p;
    if (!read_file(p, d_)) { if (error) *error = "cannot read " + p; return false; }
    if (tag_at(d_, 0) != "EMDF") { if (error) *error = "not an EP3 (EMDF) file"; return false; }
    if (!parseItems()) { if (error) *error = "bad item table"; return false; }
    for (auto& it : items) {
        if (it.tag == "EMTH") parseTexture(it);
        else if (it.tag == "EMDH") parseMaterial(it);
    }
    for (auto& it : items) if (it.tag == "EEDH") parseEntity(it);
    for (auto& it : items) {
        if (it.tag != "EODH" || it.off + 0xa0 > d_.size()) continue;
        Locator l;
        l.item = it.index; l.parent = it.parent; l.tag = it.tagName;
        std::string nm = it.path;
        size_t bar = nm.rfind('|');
        if (bar != std::string::npos) nm = nm.substr(bar + 1);
        for (const char* pre : {"EM_LOCATOR_", "EmLocator_Uid_"})
            if (nm.rfind(pre, 0) == 0) nm = nm.substr(std::strlen(pre));
        l.name = nm;
        for (int i = 0; i < 16; ++i) l.matrix[i] = befloat(&d_[it.off + 0x60 + i * 4]);
        locators.push_back(std::move(l));
    }
    for (auto& it : items) {
        if (it.tag != "ECEH" || it.off + 0xc0 > d_.size()) continue;
        Camera c;
        size_t bar = it.path.rfind('|');
        c.name = bar == std::string::npos ? it.path : it.path.substr(bar + 1);
        for (int i = 0; i < 3; ++i) c.pos[i] = befloat(&d_[it.off + 0xa0 + i * 4]);
        const float fov = befloat(&d_[it.off + 0xb8]);
        if (fov > 1 && fov < 120) c.fovDeg = fov;
        cameras.push_back(std::move(c));
    }
    for (auto& it : items) {
        if (it.tag != "ELDH" || it.off + 0xb8 > d_.size()) continue;
        Light l;
        l.item = it.index;
        l.flags = be32(&d_[it.off + 0x54]);
        for (int i = 0; i < 16; ++i) l.matrix[i] = befloat(&d_[it.off + 0x60 + i * 4]);
        for (int i = 0; i < 4; ++i) l.colour[i] = befloat(&d_[it.off + 0xa0 + i * 4]);
        l.range = befloat(&d_[it.off + 0xb4]);
        lights.push_back(l);
    }

    // world transforms
    std::map<int, size_t> byItem;
    for (size_t i = 0; i < entities.size(); ++i) byItem[entities[i].item] = i;
    std::set<int> done;
    std::function<void(size_t)> solve = [&](size_t i) {
        Entity& e = entities[i];
        if (done.count(e.item)) return;
        done.insert(e.item);
        auto pit = byItem.find(e.parent);
        if (pit != byItem.end() && pit->second != i) {
            solve(pit->second);
            matmul(e.local, entities[pit->second].world, e.world);
        } else std::memcpy(e.world, e.local, sizeof e.world);
    };
    for (size_t i = 0; i < entities.size(); ++i) solve(i);
    return true;
}

bool Package::parseItems() {
    const uint32_t hdrEnd = be32(d_.data() + 8);
    if (hdrEnd > d_.size()) return false;
    size_t o = 0x100;
    while (o + 8 <= hdrEnd && !(be32(&d_[o]) == 0 && is_tag(tag_at(d_, o)))) o += 4;
    while (o + 0x20 <= hdrEnd) {
        std::string tag = tag_at(d_, o);
        uint32_t size = be32(&d_[o + 8]);
        if (!is_tag(tag) || size == 0 || o + size > d_.size()) break;
        Item it;
        it.index = int(items.size());
        it.off = uint32_t(o); it.size = size; it.tag = tag;
        it.data = be32(&d_[o + 0x18]);
        if (tag_at(d_, o + 0x20) == "EMUK") {
            uint32_t par = be32(&d_[o + 0x28]);
            it.parent = par == 0xffffffffu ? -1 : int(par);
        }
        auto strs = strings_in(d_, o + 0x20, o + size);
        for (auto& s : strs) {
            std::string c = clean(s);
            if (c.find(":/") != std::string::npos || c.find(":\\") != std::string::npos || c.rfind("Em", 0) == 0) { it.path = c; break; }
        }
        if (!strs.empty()) { it.tagName = clean(strs.back()); if (it.tagName == "NoTag") it.tagName.clear(); }
        items.push_back(std::move(it));
        o = (o + size + 15) & ~size_t(15);
    }
    return !items.empty();
}

void Package::parseTexture(const Item& it) {
    if (!it.data) return;
    std::string warn;
    Block b = load_block(d_, it.data, &warn);
    int64_t g = b.ptr(0x94);
    if (g < 0 || !b.in(size_t(g), 24)) { warnings.push_back("texture " + std::to_string(it.index) + ": no gcm header"); return; }
    const uint8_t* q = b.p(size_t(g));
    Texture t;
    t.name = it.path;
    t.format = q[0]; t.mipmaps = q[1]; t.dimension = q[2]; t.cubemap = q[3] != 0;
    t.width = be16(q + 8); t.height = be16(q + 10); t.depth = be16(q + 12);
    t.texels = std::move(b.stream);
    if (!warn.empty()) warnings.push_back("texture " + std::to_string(it.index) + ": " + warn);
    textures[it.index] = std::move(t);
}

void Package::parseMaterial(const Item& it) {
    const size_t o = it.off;
    if (o + 0xe0 > d_.size()) return;
    Material m;
    m.name = it.path;
    size_t cc = m.name.find("::");
    if (cc != std::string::npos) m.name = m.name.substr(cc + 2);
    m.shader = be32(&d_[o + 0x50]);
    m.renderState = be32(&d_[o + 0x54]);
    uint32_t n = be32(&d_[o + 0x90]);
    for (uint32_t i = 0; i < n && i < 16 && o + 0xe0 + i * 12 + 12 <= it.off + it.size; ++i) {
        const uint8_t* s = &d_[o + 0xe0 + i * 12];
        m.slots.push_back({be32(s), int(be32(s + 8))});
    }
    materials[it.index] = std::move(m);
}

void Package::parseEntity(const Item& it) {
    Entity e;
    e.item = it.index;
    e.parent = it.parent;
    std::string nm = it.path;
    size_t bar = nm.rfind('|');
    if (bar != std::string::npos) nm = nm.substr(bar + 1);
    for (const char* pre : {"EM_ENTITY_", "EmEntity_Uid_"})
        if (nm.rfind(pre, 0) == 0) nm = nm.substr(std::strlen(pre));
    e.name = nm;
    for (int i = 0; i < 16; ++i) e.local[i] = befloat(&d_[it.off + 0x60 + i * 4]);

    if (it.data && it.data < d_.size()) {
        std::string warn;
        Block G = load_block(d_, it.data, &warn);
        std::set<int64_t> seen;
        int64_t n = 0;
        while (n >= 0 && !seen.count(n) && G.in(size_t(n), 0x130)) {
            seen.insert(n);
            const uint32_t nseg = G.u(size_t(n) + 0x128);
            const int64_t segp = G.ptr(uint32_t(n) + 0x12c);
            std::map<uint32_t, int> segmat;
            if (segp > n) {
                for (int64_t lo = n + 0x130; lo + 0x20 <= segp; lo += 16) {
                    if (G.u(lo) || G.u(lo + 4) || G.u(lo + 0xc) || G.u(lo + 0x10)) continue;
                    uint32_t v = G.u(lo + 8), m = G.u(lo + 0x14);
                    if ((v & 0xffff) && (v >> 16) < nseg && materials.count(int(m)))
                        for (uint32_t k = v >> 16; k < (v >> 16) + (v & 0xffff); ++k) segmat.emplace(k, int(m));
                }
            }
            for (uint32_t s = 0; segp >= 0 && s < nseg && s < 4096; ++s) {
                const uint32_t S = uint32_t(segp) + s * 0x100;
                if (!G.in(S, 0x100)) break;
                const uint32_t isz = G.u(S + 0x38);
                const int64_t ip = G.ptr(S + 0x3c);
                const uint32_t p0s = G.u(S + 0x68), p1s = G.u(S + 0x70);
                const int64_t p0 = G.ptr(S + 0x6c), p1 = G.ptr(S + 0x74);
                const int64_t cfgp = G.ptr(S + 0xac);
                const int64_t rdp = G.ptr(S + 0xe4);
                const uint32_t rdsz = G.u(S + 0xe0), rsize = G.u(S + 0xe8), roff = G.u(S + 0xec);
                if (ip < 0 || p0 < 0 || cfgp < 0 || !G.in(size_t(ip), isz) || !G.in(size_t(cfgp), 16)) {
                    warnings.push_back(e.name + ": segment missing pointers"); continue;
                }
                const uint8_t* cfg = G.p(size_t(cfgp));
                const uint8_t inFmt = cfg[2];
                const uint32_t nidx = be16(cfg + 10);
                if (inFmt != 0) { warnings.push_back(e.name + ": unsupported vertex input format"); continue; }
                MeshSegment ms;
                ms.material = segmat.count(s) ? segmat[s] : -1;
                if (!edge::decompress_indexes(G.p(size_t(ip)), isz, nidx, ms.indices)) {
                    warnings.push_back(e.name + ": index decompression failed"); continue;
                }
                uint32_t nv = 0;
                for (uint32_t x : ms.indices) nv = std::max(nv, x + 1);
                std::vector<uint8_t> posb;
                if (G.in(size_t(p0), p0s)) posb.insert(posb.end(), G.p(size_t(p0)), G.p(size_t(p0)) + p0s);
                if (p1 >= 0 && p1s && G.in(size_t(p1), p1s)) posb.insert(posb.end(), G.p(size_t(p1)), G.p(size_t(p1)) + p1s);
                if (size_t(nv) * 12 > posb.size()) { warnings.push_back(e.name + ": positions too short"); continue; }
                ms.positions.resize(size_t(nv) * 3);
                for (uint32_t i = 0; i < nv * 3; ++i) ms.positions[i] = befloat(&posb[i * 4]);

                if (rdp >= 0 && !G.stream.empty() && G.in(size_t(rdp), std::max<uint32_t>(rdsz, 8))) {
                    const uint8_t* rd = G.p(size_t(rdp));
                    const uint32_t nattr = rd[0], stride = rd[1];
                    if (stride && roff + size_t(nv) * stride <= G.stream.size() && 8 + nattr * 8 <= std::max<uint32_t>(rdsz, 8)) {
                        const uint8_t* vb = G.stream.data() + roff;
                        for (uint32_t a = 0; a < nattr; ++a) {
                            const uint8_t* ab = rd + 8 + a * 8;
                            const uint8_t aoff = ab[0], afmt = ab[1], acnt = ab[2], aid = ab[3];
                            if (aid == 2 && afmt == 6) {
                                ms.normals.resize(size_t(nv) * 3);
                                for (uint32_t i = 0; i < nv; ++i) decode_x11y11z10n(be32(vb + i * stride + aoff), &ms.normals[i * 3]);
                            } else if (aid >= 5 && aid <= 8 && (afmt == 3 || afmt == 2) && acnt >= 2) {
                                // UV sets 0..3 (F16 or F32)
                                auto& dst = aid == 5 ? ms.uvs : ms.uvsExtra[aid - 6];
                                dst.resize(size_t(nv) * 2);
                                for (uint32_t i = 0; i < nv; ++i) {
                                    const uint8_t* q = vb + i * stride + aoff;
                                    if (afmt == 3) { dst[i * 2] = half_to_float(be16(q)); dst[i * 2 + 1] = half_to_float(be16(q + 2)); }
                                    else { dst[i * 2] = befloat(q); dst[i * 2 + 1] = befloat(q + 4); }
                                }
                            } else if (aid == 17 && afmt == 2 && acnt >= 2) {
                                ms.particleCorner.resize(size_t(nv) * 2);
                                for (uint32_t i = 0; i < nv; ++i) {
                                    ms.particleCorner[i * 2] = befloat(vb + i * stride + aoff);
                                    ms.particleCorner[i * 2 + 1] = befloat(vb + i * stride + aoff + 4);
                                }
                            } else if ((aid == 4 || aid == 9) && afmt == 4 && acnt == 4) {
                                ms.colors.resize(size_t(nv) * 4);
                                for (uint32_t i = 0; i < nv; ++i) std::memcpy(&ms.colors[i * 4], vb + i * stride + aoff, 4);
                            }
                        }
                    }
                    (void)rsize;
                }
                e.meshes.push_back(std::move(ms));
            }
            n = G.ptr(uint32_t(n) + 0xb4);
        }
    }
    entities.push_back(std::move(e));
}

} // namespace ep3
} // namespace gti
