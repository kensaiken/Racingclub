import struct, glob, os, re, wave, math

DATA = '../../NPEB00060/USRDIR/data'
COEF = [(0, 0), (60 / 64, 0), (115 / 64, -52 / 64), (98 / 64, -55 / 64), (122 / 64, -60 / 64)]

def decode(path):
    d = open(path, 'rb').read()
    size, rate = struct.unpack_from('>II', d, 0x0c)
    out = []; h1 = h2 = 0
    for o in range(0x30, min(len(d), 0x30 + size) - 15, 16):
        pred = min(d[o] >> 4, 4); shift = d[o] & 15
        if d[o + 1] == 7: break
        for i in range(28):
            b = d[o + 2 + i // 2]
            n = (b >> 4) if i & 1 else (b & 15)
            if n & 8: n -= 16
            s = ((n << 12) >> shift) + h1 * COEF[pred][0] + h2 * COEF[pred][1]
            h2, h1 = h1, s
            out.append(max(-32768, min(32767, int(s))))
    return rate, out

def stats(x):
    rms = math.sqrt(sum(v * v for v in x) / len(x)) / 32768
    zcr = sum(1 for a, b in zip(x, x[1:]) if (a < 0) != (b < 0)) / len(x)
    return rms, zcr

# 1) decode sanity
for f in ['cars/audiodata/engines/oldmini/tag_oldmini_loop05_on_en.vag', 'cars/audiodata/engines/oldmini/tag_oldmini_idle_ex.vag', 'audio/misc/churchbelltollloop.vag']:
    p = os.path.join(DATA, f)
    rate, x = decode(p)
    rms, zcr = stats(x)
    print('%-60s rate=%d n=%d rms=%.3f zcr=%.3f' % (f, rate, len(x), rms, zcr))
    if 'loop05' in f:
        w = wave.open(r'C:/Users/User/Desktop/GTi Club/GTiPort/reference/engine_sample_test.wav', 'wb')
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(b''.join(struct.pack('<h', v) for v in x)); w.close()

# 2) every referenced file exists
missing = 0; total = 0
for xml in glob.glob(DATA + '/cars/pack_*/*/*_full_audio.xml'):
    s = open(xml, encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'<audionode name="([^"]+)">.*?<file path="([^"]+)"', s, re.S):
        name, path = m.group(1), m.group(2).replace(chr(92), '/')
        if path.lower().startswith('data/'): path = path[5:]
        f = os.path.join(DATA, path.lower(), name.lower() + '.vag'); total += 1
        if not os.path.exists(f): missing += 1; print('MISSING', os.path.basename(xml), f)
print('referenced', total, 'missing', missing)
