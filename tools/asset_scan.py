#!/usr/bin/env python3
"""asset_scan.py - classify GTI Club (PS3, NPEB00060) files by magic number.

usage:  python -I asset_scan.py <root_dir> [--embedded] [--csv out.csv]

  * Classifies every file by its header signature (not by extension).
  * --embedded additionally scans the *body* of each file for well-known container
    signatures (ELF / DDS / PNG / RIFF / VAG / zlib) to find nested assets.
  * Prints a per-format summary and the list of files with unknown headers.

All PS3 formats are big-endian; the Sumo Digital 'Emmersion' engine stores its
4CC tags byte-reversed (e.g. the bytes 'FDME' at +4 mean tag 'EMDF').
"""
import argparse, collections, csv, os, re, struct, sys

# (name, offset, magic bytes, description)
SIGNATURES = [
    ('EP3/Emmersion package', 4, b'FDME', 'Sumo Emmersion package: textures, materials, meshes (Edge geometry)'),
    ('SCE SELF/SPRX',         0, b'SCE\x00', 'Signed/encrypted PS3 executable (EBOOT.BIN, .sprx, .self)'),
    ('ELF',                   0, b'\x7fELF', 'Plain ELF (decrypted EBOOT or SPU image)'),
    ('NPD/EDAT',              0, b'NPD\x00', 'NPDRM-protected data file (.edat/.sdat)'),
    ('PARAM.SFO',             0, b'\x00PSF', 'PS3 system file object (title id, version)'),
    ('TROPHY.TRP',            0, b'\xdc\xa2\x4d\x00', 'Trophy package'),
    ('VAG (PS ADPCM)',        0, b'VAGp', 'Sony ADPCM sound effect (mono)'),
    ('MSF audio',             0, b'MSF',  'Sony MultiStream File (ADPCM/MP3/ATRAC3 stream)'),
    ('RIFF/WAVE (AT3?)',      0, b'RIFF', 'RIFF container: PCM .wav or ATRAC3 .at3'),
    ('Emmersion RELO (UI)',   0, b'RELO', 'Sumo UI relocatable layout (.rel)'),
    ('Emmersion INFO (UI)',   0, b'INFO', 'Sumo UI data (.dat)'),
    ('DDS',                   0, b'DDS ', 'DirectDraw Surface texture'),
    ('PNG',                   0, b'\x89PNG', 'PNG image'),
    ('XML',                   0, b'<?xml', 'XML (physics, Emmersion project .evp/.evw)'),
    ('Ogg',                   0, b'OggS', 'Ogg stream'),
    ('PAMF/PSMF video',       0, b'PAMF', 'PS3 movie'),
    ('zlib',                  0, b'\x78\x9c', 'zlib stream'),
]

EMBEDDED = [(b'\x7fELF', 'ELF'), (b'DDS ', 'DDS'), (b'\x89PNG\r\n', 'PNG'), (b'RIFF', 'RIFF'),
            (b'VAGp', 'VAG'), (b'%SPURS JOB INFO%', 'SPURS job')]

def classify(head):
    for name, off, magic, _ in SIGNATURES:
        if head[off:off + len(magic)] == magic:
            return name
    if head[:4] == b'\x00\x00\x00\x00' and re.fullmatch(rb'[A-Z]{4}', head[4:8] or b''):
        return 'Emmersion chunk (%s)' % head[4:8][::-1].decode()
    if head and all(32 <= c < 127 or c in (9, 10, 13) for c in head[:64]):
        return 'Text'
    return None

def describe_extra(name, path, head):
    """Small format-specific details for the report."""
    try:
        if name == 'VAG (PS ADPCM)':
            size, rate = struct.unpack_from('>II', head, 0x0c)
            return 'rate=%dHz data=%dB' % (rate, size)
        if name == 'MSF audio':
            codec, ch, size, rate = struct.unpack_from('>IIII', head, 4)
            return 'codec=%d ch=%d rate=%d' % (codec, ch, rate)
        if name == 'EP3/Emmersion package':
            return 'header_region=0x%x' % struct.unpack_from('>I', head, 8)[0]
        if name == 'SCE SELF/SPRX':
            return 'header_type=%d' % struct.unpack_from('>H', head, 0x0a)[0]
    except struct.error:
        pass
    return ''

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('root')
    ap.add_argument('--embedded', action='store_true')
    ap.add_argument('--csv')
    a = ap.parse_args()

    counts = collections.Counter(); sizes = collections.Counter(); exts = collections.defaultdict(collections.Counter)
    unknown = []; rows = []; embedded = collections.Counter()
    for dp, _, fns in os.walk(a.root):
        for fn in fns:
            p = os.path.join(dp, fn)
            try:
                with open(p, 'rb') as f:
                    head = f.read(64)
                    body = f.read() if a.embedded else b''
            except OSError as ex:
                print('skip', p, ex, file=sys.stderr); continue
            name = classify(head) or 'UNKNOWN'
            sz = os.path.getsize(p)
            counts[name] += 1; sizes[name] += sz
            exts[name][os.path.splitext(fn)[1].lower() or '(none)'] += 1
            if name == 'UNKNOWN': unknown.append((p, head[:16].hex()))
            if a.embedded and body:
                for sig, label in EMBEDDED:
                    n = body.count(sig)
                    if n: embedded[(label, os.path.relpath(p, a.root))] += n
            rows.append((os.path.relpath(p, a.root), name, sz, describe_extra(name, p, head)))

    print('%-34s %7s %10s  extensions' % ('format', 'files', 'MB'))
    for name, n in counts.most_common():
        ex = ', '.join('%s:%d' % kv for kv in exts[name].most_common(5))
        print('%-34s %7d %10.1f  %s' % (name, n, sizes[name] / 1e6, ex))
    if unknown:
        print('\nUNKNOWN headers (%d):' % len(unknown))
        for p, h in unknown[:40]: print('  %s  %s' % (h, os.path.relpath(p, a.root)))
    if embedded:
        print('\nEmbedded signatures (outside file header):')
        for (label, p), n in embedded.most_common(30): print('  %-10s x%-5d %s' % (label, n, p))
    if a.csv:
        with open(a.csv, 'w', newline='') as f:
            w = csv.writer(f); w.writerow(['path', 'format', 'bytes', 'details']); w.writerows(rows)
        print('\nCSV written:', a.csv)

if __name__ == '__main__':
    main()
