"""Static recompiler: PPU (PowerPC 64, Cell) machine code of the decrypted EBOOT -> C++.

usage: python ppc2cpp.py out.cpp <lo_hex>-<hi_hex> [<lo>-<hi> ...]
Every function whose start lies in the ranges is translated to `void f_XXXXXXXX(PPU& c)`.
Branches inside a function become gotos; calls go through rc_call() (direct calls are resolved to the generated
function when it exists). The runtime (src/recomp/ppu.h) provides registers, big-endian guest memory and helpers.

Generated code is derived from the user's own game binary and must not be distributed.
"""
import pickle, re, sys, os, bisect

RE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 're')
ins = pickle.load(open(os.path.join(RE, 'ppu_dis.pkl'), 'rb'))
addrs = [a for a, m, o in ins]
idx = {a: i for i, a in enumerate(addrs)}

starts = set(a for a, m, o in ins if m in ('stwu', 'stdu') and o.startswith('r1, -'))
for a, m, o in ins:
    if m == 'bl':
        try: starts.add(int(o, 16))
        except ValueError: pass
starts = sorted(set(starts) | set(int(x, 16) for x in open(os.path.join(RE, 'opd_targets.txt')).read().split())
                | set(int(x, 16) for x in open(os.path.join(RE, 'extra_entries.txt')).read().split()))
STRONG = set(starts)   # real entry points: prologues, call targets, descriptors; a branch to one is a tail call
# branches (b / bc) that leave their function become entry points too (tail calls into the middle of code)
_extra = set()
for _a, _m, _o in ins:
    if _m.startswith('b') and _m not in ('bl', 'blr', 'bctr', 'bctrl') and _o:
        _t = _o.split(',')[-1].strip()
        if _t.startswith('0x'):
            _tv = int(_t, 16)
            _k = bisect.bisect_right(starts, _a)
            _fs = starts[_k - 1] if _k else 0
            _fe = starts[_k] if _k < len(starts) else _a + 4
            if not (_fs <= _tv < _fe): _extra.add(_tv)
starts = sorted(set(starts) | _extra)

REG = re.compile(r'^r(\d+)$')
FREG = re.compile(r'^f(\d+)$')
VREG = re.compile(r'^v(\d+)$')
MEM = re.compile(r'^(-?(?:0x[0-9a-f]+|\d+))\((r\d+|0)\)$')


def num(s):
    return int(s, 0)


def R(x):
    return 'c.r[%s]' % REG.match(x).group(1)


def F(x):
    return 'c.f[%s]' % FREG.match(x).group(1)


def V(x):
    return 'c.v[%s]' % VREG.match(x).group(1)


def ea(base, disp):
    # (rA|0) + d, 32-bit guest addresses
    b = '0' if base in ('r0', '0') else '(uint32_t)%s' % R(base)
    return '(uint32_t)(%s + (int32_t)%d)' % (b, disp)


def eax(ra, rb):
    a = '0' if ra in ('r0', '0') else '(uint32_t)%s' % R(ra)
    return '(uint32_t)(%s + (uint32_t)%s)' % (a, R(rb))


CRBITS = {'lt': 0, 'gt': 1, 'eq': 2, 'so': 3, 'un': 3}


class Unsupported(Exception):
    pass


def cond_branch(m, ops, nxt):
    """Return (condition expression, target or None for lr/ctr kind, kind)."""
    base = m.rstrip('+-')
    kind = 'b'
    if base.endswith('lr'):
        kind = 'lr'; base = base[:-2]
    elif base.endswith('ctr') and base not in ('bdnz', 'bdz'):
        kind = 'ctr'; base = base[:-3]
    link = False
    if base.endswith('l') and base not in ('bl',) and base[:-1] in ('beq', 'bne', 'blt', 'bgt', 'ble', 'bge', 'bdnz', 'bdz'):
        link = True; base = base[:-1]
    crf = 0
    tgt = None
    if ops and ops[0].startswith('cr'):
        crf = int(ops[0][2:]); ops = ops[1:]
    if kind == 'b':
        tgt = num(ops[-1])
    bit = lambda b: '((c.cr >> (31 - %d)) & 1)' % (crf * 4 + b)
    table = {
        'beq': bit(2), 'bne': '!' + bit(2), 'blt': bit(0), 'bge': '!' + bit(0),
        'bgt': bit(1), 'ble': '!' + bit(1), 'bun': bit(3), 'bnu': '!' + bit(3),
    }
    if base == 'bdnz':
        return '(--c.ctr != 0)', tgt, kind, link
    if base == 'bdz':
        return '(--c.ctr == 0)', tgt, kind, link
    if base not in table:
        raise Unsupported(m)
    return table[base], tgt, kind, link


# Game code changes, applied to the C++ of one instruction: {address: (text to find, replacement, why)}.
# The game was written for 60 images per second; these make per-image steps follow the emulated vblank rate
# (g_frameStep = 60 / rate, set by the runtime; 1.0 at 60 Hz, so the original behaviour is unchanged there).
PATCHES = {
    # car update: entity+100 = time step of the car's per-image logic (timers, effects), constant 1.0 per image
    0x224be0: ('c.wrf32(ea_, (float)c.f[0]);', 'c.wrf32(ea_, (float)c.f[0] * g_frameStep);', 'car per-image step'),
}


def tr_patched(a, m, o, fstart, fend, labels):
    S = tr(a, m, o, fstart, fend, labels)
    if a in PATCHES:
        find, repl, why = PATCHES[a]
        hit = [st for st in S if find in st]
        if len(hit) != 1:
            raise SystemExit('patch at %x (%s) does not apply: %r' % (a, why, S))
        S = [st.replace(find, repl) + ('   // PATCH: %s' % why if find in st else '') for st in S]
    return S


def tr(a, m, o, fstart, fend, labels):
    """Translate one instruction to C++ statements."""
    ops = [x.strip() for x in o.split(',')] if o else []
    nxt = a + 4
    S = []
    e = S.append

    def call(target_expr, tail=False):
        if tail:
            e('{ rc_call(c, %s); return; }' % target_expr)
        else:
            e('c.lr = 0x%x; rc_call(c, %s);' % (nxt, target_expr))

    # ---- branches ----
    if m == 'b':
        t = num(ops[0])
        if t in labels: e('goto L_%x;' % t)
        else: call('0x%x' % t, tail=True)
        return S
    if m == 'bl':
        call('0x%x' % num(ops[0])); return S
    if m == 'blr':
        e('return;'); return S
    if m == 'bctr':
        if JT:   # switch tables: an indirect jump inside the function goes to one of its block starts
            e('switch ((uint32_t)c.ctr) { %s default: break; }' % ' '.join('case 0x%x: goto L_%x;' % (x, x) for x in sorted(JT)))
        call('(uint32_t)c.ctr', tail=True); return S
    if m == 'bctrl':
        call('(uint32_t)c.ctr'); return S
    if m.rstrip('+-') in ('beq', 'bne', 'blt', 'bgt', 'ble', 'bge', 'bdnz', 'bdz', 'bun', 'bnu',
                          'beqlr', 'bnelr', 'bltlr', 'bgtlr', 'blelr', 'bgelr', 'beqctr', 'bnectr', 'beql', 'bnel'):
        cnd, tgt, kind, link = cond_branch(m, ops, nxt)
        if kind == 'lr':
            e('if (%s) return;' % cnd)
        elif kind == 'ctr':
            e('if (%s) { rc_call(c, (uint32_t)c.ctr); return; }' % cnd)
        elif link:
            e('if (%s) { c.lr = 0x%x; rc_call(c, 0x%x); }' % (cnd, nxt, tgt))
        elif tgt in labels:
            e('if (%s) goto L_%x;' % (cnd, tgt))
        else:
            e('if (%s) { rc_call(c, 0x%x); return; }' % (cnd, tgt))
        return S

    # ---- special registers / CR ----
    if m == 'sc': e('rc_syscall(c);'); return S
    if m in ('tdi', 'twi', 'td', 'tw') or m.startswith(('tdl', 'tdg', 'tde', 'tdn', 'twl', 'twg', 'twe', 'twn')):
        return S                                   # conditional traps (debug asserts): ignored
    if m in ('stmw', 'lmw'):
        rs = int(REG.match(ops[0]).group(1)); mm2 = MEM.match(ops[1])
        base = ea(mm2.group(2), num(mm2.group(1)))
        for k in range(rs, 32):
            if m == 'stmw': e('c.wr32(%s + %d, c.r[%d]);' % (base, (k - rs) * 4, k))
            else: e('c.r[%d] = c.rd32(%s + %d);' % (k, base, (k - rs) * 4))
        return S
    if m in ('addic', 'addic.'):
        e('{ uint64_t a_ = %s; uint64_t r_ = a_ + (uint64_t)(int64_t)%d; c.xer_ca = r_ < a_; %s = r_; }' % (R(ops[1]), num(ops[2]), R(ops[0])))
        if m == 'addic.': e('c.cmp_s(0, (int64_t)%s, 0);' % R(ops[0]))
        return S
    if m in ('rlwnm', 'rlwnm.', 'rotlw', 'rotlw.') and len(ops) == 5:
        e('%s = c.rotl32((uint32_t)%s, (int)(%s & 31)) & c.mask32(%d, %d);' % (R(ops[0]), R(ops[1]), R(ops[2]), num(ops[3]), num(ops[4])))
        if m.endswith('.'): e('c.cmp_s(0, (int64_t)%s, 0);' % R(ops[0]))
        return S
    if m in ('stdux', 'stwux', 'ldux', 'lwzux', 'stbux', 'lbzux'):
        fn = {'stdux': 'wr64', 'stwux': 'wr32', 'stbux': 'wr8', 'ldux': 'rd64', 'lwzux': 'rd32', 'lbzux': 'rd8'}[m]
        if m.startswith('st'): e('{ uint32_t ea_ = %s; c.%s(ea_, %s); %s = ea_; }' % (eax(ops[1], ops[2]), fn, R(ops[0]), R(ops[1])))
        else: e('{ uint32_t ea_ = %s; %s = c.%s(ea_); %s = ea_; }' % (eax(ops[1], ops[2]), R(ops[0]), fn, R(ops[1])))
        return S   # lv2 system call: number in r11, args r3..r10, result r3
    if m in ('mftb', 'mftbu'): e('%s = %s;' % (R(ops[0]), 'rc_timebase()' if m == 'mftb' else '(rc_timebase() >> 32)')); return S
    if m == 'mflr': e('%s = c.lr;' % R(ops[0])); return S
    if m == 'mtlr': e('c.lr = %s;' % R(ops[0])); return S
    if m == 'mtctr': e('c.ctr = %s;' % R(ops[0])); return S
    if m == 'mfctr': e('%s = c.ctr;' % R(ops[0])); return S
    if m == 'mfcr': e('%s = c.cr;' % R(ops[0])); return S
    if m == 'mfocrf':
        e('%s = c.cr;' % R(ops[0])); return S
    if m == 'mtocrf':
        mask = num(ops[0])
        e('{ uint32_t msk = 0; for (int k = 0; k < 8; ++k) if (0x%x & (0x80 >> k)) msk |= 0xFu << (28 - 4 * k); c.cr = (c.cr & ~msk) | ((uint32_t)%s & msk); }' % (mask, R(ops[1])))
        return S
    if m == 'mcrf':
        d, s_ = int(ops[0][2:]), int(ops[1][2:])
        e('c.set_crf(%d, (c.cr >> (28 - 4 * %d)) & 0xF);' % (d, s_)); return S
    if m in ('cror', 'crnot', 'crand', 'crxor', 'crnor', 'crandc', 'creqv', 'crorc', 'crnand', 'crset', 'crclr'):
        def crb(x):
            mm = re.match(r'cr(\d)(lt|gt|eq|so|un)', x)
            if mm: return int(mm.group(1)) * 4 + CRBITS[mm.group(2)]
            return CRBITS[x] if x in CRBITS else num(x)
        bits = [crb(x) for x in ops]
        g = lambda b: '((c.cr >> (31 - %d)) & 1)' % b
        if m == 'crnot': val = '(!%s)' % g(bits[1])
        elif m == 'cror': val = '(%s | %s)' % (g(bits[1]), g(bits[2]))
        elif m == 'crand': val = '(%s & %s)' % (g(bits[1]), g(bits[2]))
        elif m == 'crxor': val = '(%s ^ %s)' % (g(bits[1]), g(bits[2]))
        elif m == 'crnor': val = '(!(%s | %s))' % (g(bits[1]), g(bits[2]))
        elif m == 'crandc': val = '(%s & !%s)' % (g(bits[1]), g(bits[2]))
        elif m == 'creqv': val = '(!(%s ^ %s))' % (g(bits[1]), g(bits[2]))
        elif m == 'crorc': val = '(%s | !%s)' % (g(bits[1]), g(bits[2]))
        elif m == 'crnand': val = '(!(%s & %s))' % (g(bits[1]), g(bits[2]))
        elif m == 'crset': val = '1'
        else: val = '0'
        e('c.set_crbit(%d, %s);' % (bits[0], val)); return S

    # ---- compares ----
    if m in ('cmpwi', 'cmpw', 'cmplwi', 'cmplw', 'cmpdi', 'cmpd', 'cmpldi', 'cmpld'):
        crf = 0
        if ops[0].startswith('cr'): crf = int(ops[0][2:]); ops = ops[1:]
        a_ = R(ops[0])
        b_ = R(ops[1]) if ops[1].startswith('r') else str(num(ops[1]))
        if m in ('cmpwi', 'cmpw'): e('c.cmp_s(%d, (int64_t)(int32_t)%s, (int64_t)(int32_t)(%s));' % (crf, a_, b_))
        elif m in ('cmplwi', 'cmplw'): e('c.cmp_u(%d, (uint32_t)%s, (uint32_t)(%s));' % (crf, a_, b_))
        elif m in ('cmpdi', 'cmpd'): e('c.cmp_s(%d, (int64_t)%s, (int64_t)(%s));' % (crf, a_, b_))
        else: e('c.cmp_u(%d, (uint64_t)%s, (uint64_t)(%s));' % (crf, a_, b_))
        return S
    if m in ('fcmpu', 'fcmpo'):
        crf = int(ops[0][2:])
        e('c.fcmp(%d, %s, %s);' % (crf, F(ops[1]), F(ops[2]))); return S

    # ---- integer ----
    rec = m.endswith('.') and m not in ('stwcx.', 'stdcx.')
    mm_ = m.rstrip('.')
    def setrec(r):
        if rec: e('c.cmp_s(0, (int64_t)%s, 0);' % r)
    if mm_ == 'li': e('%s = (uint64_t)(int64_t)%d;' % (R(ops[0]), num(ops[1]))); return S
    if mm_ == 'lis': e('%s = (uint64_t)(int64_t)(int32_t)(%d << 16);' % (R(ops[0]), num(ops[1]) & 0xffff)); return S
    if mm_ == 'addc': e('{ uint64_t a_ = %s; %s = a_ + %s; c.xer_ca = %s < a_; }' % (R(ops[1]), R(ops[0]), R(ops[2]), R(ops[0]))); return S
    if mm_ == 'adde': e('{ uint64_t a_ = %s, b_ = %s; uint64_t r_ = a_ + b_ + c.xer_ca; c.xer_ca = (r_ < a_) || (c.xer_ca && r_ == a_); %s = r_; }' % (R(ops[1]), R(ops[2]), R(ops[0]))); return S
    if mm_ == 'subfc': e('{ uint64_t a_ = %s, b_ = %s; c.xer_ca = b_ >= a_; %s = b_ - a_; }' % (R(ops[1]), R(ops[2]), R(ops[0]))); return S
    if mm_ == 'subfe': e('{ uint64_t a_ = ~%s, b_ = %s; uint64_t r_ = a_ + b_ + c.xer_ca; c.xer_ca = (r_ < a_) || (c.xer_ca && r_ == a_); %s = r_; }' % (R(ops[1]), R(ops[2]), R(ops[0]))); return S
    if mm_ == 'divw': e('%s = (int32_t)%s == 0 ? 0 : (uint64_t)(int64_t)((int32_t)%s / (int32_t)%s);' % (R(ops[0]), R(ops[2]), R(ops[1]), R(ops[2]))); return S
    if mm_ == 'divwu': e('%s = (uint32_t)%s == 0 ? 0 : (uint64_t)((uint32_t)%s / (uint32_t)%s);' % (R(ops[0]), R(ops[2]), R(ops[1]), R(ops[2]))); return S
    if mm_ == 'divd': e('%s = (int64_t)%s == 0 ? 0 : (uint64_t)((int64_t)%s / (int64_t)%s);' % (R(ops[0]), R(ops[2]), R(ops[1]), R(ops[2]))); return S
    if mm_ == 'divdu': e('%s = %s == 0 ? 0 : %s / %s;' % (R(ops[0]), R(ops[2]), R(ops[1]), R(ops[2]))); return S
    if mm_ == 'mulld': e('%s = (uint64_t)((int64_t)%s * (int64_t)%s);' % (R(ops[0]), R(ops[1]), R(ops[2]))); return S
    if mm_ == 'mr': e('%s = %s;' % (R(ops[0]), R(ops[1]))); setrec(R(ops[0])); return S
    if mm_ == 'nop': return S
    if mm_ in ('addi', 'addis'):
        imm = num(ops[2]) if mm_ == 'addi' else ((num(ops[2]) & 0xffff) << 16) - (0x100000000 if num(ops[2]) & 0x8000 else 0)
        base = '0' if ops[1] == 'r0' else R(ops[1])
        e('%s = %s + (uint64_t)(int64_t)%d;' % (R(ops[0]), base, imm)); return S
    if mm_ == 'add': e('%s = %s + %s;' % (R(ops[0]), R(ops[1]), R(ops[2]))); setrec(R(ops[0])); return S
    if mm_ == 'subf': e('%s = %s - %s;' % (R(ops[0]), R(ops[2]), R(ops[1]))); setrec(R(ops[0])); return S
    if mm_ == 'subfic':
        e('{ uint64_t b_ = (uint64_t)(int64_t)%d; c.xer_ca = b_ >= %s; %s = b_ - %s; }' % (num(ops[2]), R(ops[1]), R(ops[0]), R(ops[1]))); return S
    if mm_ == 'addze': e('{ uint64_t a_ = %s; %s = a_ + c.xer_ca; c.xer_ca = (a_ + c.xer_ca) < a_; }' % (R(ops[1]), R(ops[0]))); return S
    if mm_ == 'neg': e('%s = (uint64_t)(-(int64_t)%s);' % (R(ops[0]), R(ops[1]))); return S
    if mm_ == 'mulli': e('%s = (uint64_t)((int64_t)%s * %d);' % (R(ops[0]), R(ops[1]), num(ops[2]))); return S
    if mm_ == 'mullw': e('%s = (uint64_t)((int64_t)(int32_t)%s * (int64_t)(int32_t)%s);' % (R(ops[0]), R(ops[1]), R(ops[2]))); setrec(R(ops[0])); return S
    if mm_ == 'mulhw': e('%s = (uint64_t)(int64_t)(int32_t)(((int64_t)(int32_t)%s * (int64_t)(int32_t)%s) >> 32);' % (R(ops[0]), R(ops[1]), R(ops[2]))); return S
    if mm_ == 'mulhwu': e('%s = (uint32_t)(((uint64_t)(uint32_t)%s * (uint64_t)(uint32_t)%s) >> 32);' % (R(ops[0]), R(ops[1]), R(ops[2]))); return S
    if mm_ in ('and', 'or', 'xor', 'andc', 'nor'):
        a_, b_ = R(ops[1]), R(ops[2])
        ex = {'and': '%s & %s', 'or': '%s | %s', 'xor': '%s ^ %s', 'andc': '%s & ~%s', 'nor': '~(%s | %s)'}[mm_] % (a_, b_)
        e('%s = %s;' % (R(ops[0]), ex)); setrec(R(ops[0])); return S
    if mm_ in ('ori', 'oris', 'xori', 'xoris', 'andi', 'andis'):
        imm = num(ops[2]) & 0xffff
        if mm_.endswith('is'): imm <<= 16
        op = {'ori': '|', 'oris': '|', 'xori': '^', 'xoris': '^', 'andi': '&', 'andis': '&'}[mm_]
        e('%s = %s %s 0x%xull;' % (R(ops[0]), R(ops[1]), op, imm))
        if mm_.startswith('and'): e('c.cmp_s(0, (int64_t)%s, 0);' % R(ops[0]))
        return S
    if mm_ == 'extsw': e('%s = (uint64_t)(int64_t)(int32_t)%s;' % (R(ops[0]), R(ops[1]))); setrec(R(ops[0])); return S
    if mm_ == 'extsb': e('%s = (uint64_t)(int64_t)(int8_t)%s;' % (R(ops[0]), R(ops[1]))); return S
    if mm_ == 'extsh': e('%s = (uint64_t)(int64_t)(int16_t)%s;' % (R(ops[0]), R(ops[1]))); return S
    if mm_ == 'cntlzw': e('%s = c.cntlzw((uint32_t)%s);' % (R(ops[0]), R(ops[1]))); return S
    if mm_ == 'cntlzd': e('%s = %s ? (uint64_t)__builtin_clzll(%s) : 64;' % (R(ops[0]), R(ops[1]), R(ops[1]))); setrec(R(ops[0])); return S
    if mm_ == 'clrldi': e('%s = %s & (~0ull >> %d);' % (R(ops[0]), R(ops[1]), num(ops[2]))); return S
    if mm_ in ('rlwinm', 'clrlwi', 'slwi', 'srwi', 'rotlwi'):
        if mm_ == 'rlwinm': sh, mb, me = num(ops[2]), num(ops[3]), num(ops[4])
        elif mm_ == 'clrlwi': sh, mb, me = 0, num(ops[2]), 31
        elif mm_ == 'slwi': sh, mb, me = num(ops[2]), 0, 31 - num(ops[2])
        elif mm_ == 'srwi': sh, mb, me = (32 - num(ops[2])) & 31, num(ops[2]), 31
        else: sh, mb, me = num(ops[2]), 0, 31
        e('%s = c.rlw((uint32_t)%s, %d, %d, %d);' % (R(ops[0]), R(ops[1]), sh, mb, me)); setrec(R(ops[0])); return S
    if mm_ == 'rlwimi':
        e('%s = c.rlwimi((uint32_t)%s, (uint32_t)%s, %d, %d, %d);' % (R(ops[0]), R(ops[0]), R(ops[1]), num(ops[2]), num(ops[3]), num(ops[4]))); return S
    if mm_ in ('slw', 'srw', 'sraw', 'srawi', 'sld', 'srd'):
        if mm_ == 'slw': e('{ uint32_t s_ = %s & 63; %s = s_ > 31 ? 0 : (uint32_t)((uint32_t)%s << s_); }' % (R(ops[2]), R(ops[0]), R(ops[1])))
        elif mm_ == 'srw': e('{ uint32_t s_ = %s & 63; %s = s_ > 31 ? 0 : ((uint32_t)%s >> s_); }' % (R(ops[2]), R(ops[0]), R(ops[1])))
        elif mm_ == 'sraw': e('%s = c.sraw((int32_t)%s, (uint32_t)%s & 63);' % (R(ops[0]), R(ops[1]), R(ops[2])))
        elif mm_ == 'srawi': e('%s = c.sraw((int32_t)%s, %d);' % (R(ops[0]), R(ops[1]), num(ops[2])))
        elif mm_ == 'sld': e('{ uint32_t s_ = %s & 127; %s = s_ > 63 ? 0 : (%s << s_); }' % (R(ops[2]), R(ops[0]), R(ops[1])))
        else: e('{ uint32_t s_ = %s & 127; %s = s_ > 63 ? 0 : (%s >> s_); }' % (R(ops[2]), R(ops[0]), R(ops[1])))
        setrec(R(ops[0])); return S
    if mm_ == 'rotlw': e('%s = c.rotl32((uint32_t)%s, (int)(%s & 31));' % (R(ops[0]), R(ops[1]), R(ops[2]))); setrec(R(ops[0])); return S
    if mm_ == 'rldic': e('%s = c.rotl64(%s, %d) & ((~0ull >> %d) & (~0ull << %d));' % (R(ops[0]), R(ops[1]), num(ops[2]), num(ops[3]), num(ops[2]))); return S
    if mm_ in ('rldicl', 'rldicr', 'rldimi', 'rotldi', 'sldi', 'srdi'):
        if mm_ == 'rotldi': e('%s = c.rotl64(%s, %d);' % (R(ops[0]), R(ops[1]), num(ops[2])))
        elif mm_ == 'sldi': e('%s = %s << %d;' % (R(ops[0]), R(ops[1]), num(ops[2])))
        elif mm_ == 'srdi': e('%s = %s >> %d;' % (R(ops[0]), R(ops[1]), num(ops[2])))
        elif mm_ == 'rldicl': e('%s = c.rotl64(%s, %d) & (~0ull >> %d);' % (R(ops[0]), R(ops[1]), num(ops[2]), num(ops[3])))
        elif mm_ == 'rldicr': e('%s = c.rotl64(%s, %d) & (~0ull << (63 - %d));' % (R(ops[0]), R(ops[1]), num(ops[2]), num(ops[3])))
        elif mm_ == 'rldimi': e('%s = c.rldimi(%s, %s, %d, %d);' % (R(ops[0]), R(ops[0]), R(ops[1]), num(ops[2]), num(ops[3])))
        else: raise Unsupported(m)
        return S

    # ---- loads / stores (integer) ----
    LD = {'lwz': ('rd32', 'uint64_t)(uint32_t'), 'lbz': ('rd8', 'uint64_t)(uint8_t'), 'lhz': ('rd16', 'uint64_t)(uint16_t'),
          'ld': ('rd64', 'uint64_t'), 'lha': ('rd16', 'uint64_t)(int64_t)(int16_t'), 'lwa': ('rd32', 'uint64_t)(int64_t)(int32_t')}
    ST = {'stw': 'wr32', 'stb': 'wr8', 'sth': 'wr16', 'std': 'wr64'}
    base_m = mm_[:-1] if mm_.endswith('u') and mm_[:-1] in list(LD) + list(ST) else mm_
    upd = base_m != mm_
    xform = False
    if base_m.endswith('x') and base_m[:-1] in list(LD) + list(ST) + ['lwarx', 'ldarx']:
        base_m = base_m[:-1]; xform = True
    if base_m in LD and (mm_ in LD or upd or xform):
        fn, cast = LD[base_m]
        if xform: addr = eax(ops[1], ops[2])
        else:
            mm2 = MEM.match(ops[1]); addr = ea(mm2.group(2), num(mm2.group(1)))
        e('{ uint32_t ea_ = %s; %s = (%s)c.%s(ea_);%s }' % (addr, R(ops[0]), cast, fn, (' %s = ea_;' % R(MEM.match(ops[1]).group(2))) if upd else ''))
        return S
    if base_m in ST:
        fn = ST[base_m]
        if xform: addr = eax(ops[1], ops[2])
        else:
            mm2 = MEM.match(ops[1]); addr = ea(mm2.group(2), num(mm2.group(1)))
        e('{ uint32_t ea_ = %s; c.%s(ea_, %s);%s }' % (addr, fn, R(ops[0]), (' %s = ea_;' % R(MEM.match(ops[1]).group(2))) if upd else ''))
        return S
    if mm_ in ('lwarx', 'ldarx'):
        e('%s = c.%s(%s);' % (R(ops[0]), mm_, eax(ops[1], ops[2]))); return S
    if m in ('stwcx.', 'stdcx.'):
        e('c.set_crf(0, (c.%s(%s, %s) ? 0x2 : 0) | (c.xer_so ? 1 : 0));' % (m[:-1], eax(ops[1], ops[2]), R(ops[0]))); return S

    # ---- floating point loads / stores ----
    if mm_ in ('lfs', 'lfsu', 'lfsx', 'lfd', 'lfdu', 'lfdx'):
        dbl = mm_.startswith('lfd')
        if mm_.endswith('x'): addr = eax(ops[1], ops[2]); upd_r = None
        else:
            mm2 = MEM.match(ops[1]); addr = ea(mm2.group(2), num(mm2.group(1))); upd_r = mm2.group(2) if mm_.endswith('u') else None
        e('{ uint32_t ea_ = %s; %s = %s;%s }' % (addr, F(ops[0]), 'c.rdf64(ea_)' if dbl else '(double)c.rdf32(ea_)', (' %s = ea_;' % R(upd_r)) if upd_r else ''))
        return S
    if mm_ in ('stfs', 'stfsu', 'stfsx', 'stfd', 'stfdu', 'stfdx', 'stfiwx'):
        if mm_ == 'stfiwx':
            e('c.wr32(%s, (uint32_t)c.fbits(%s));' % (eax(ops[1], ops[2]), F(ops[0]))); return S
        dbl = mm_.startswith('stfd')
        if mm_.endswith('x'): addr = eax(ops[1], ops[2]); upd_r = None
        else:
            mm2 = MEM.match(ops[1]); addr = ea(mm2.group(2), num(mm2.group(1))); upd_r = mm2.group(2) if mm_.endswith('u') else None
        e('{ uint32_t ea_ = %s; %s;%s }' % (addr, ('c.wrf64(ea_, %s)' if dbl else 'c.wrf32(ea_, (float)%s)') % F(ops[0]), (' %s = ea_;' % R(upd_r)) if upd_r else ''))
        return S

    # ---- cache / ordering hints (no effect on a single host thread) ----
    # memory barriers: other host threads (RSX command processor, SPU jobs) read what this thread stores
    if mm_ == 'sync':
        e('std::atomic_thread_fence(std::memory_order_seq_cst);'); return S
    if mm_ in ('lwsync', 'eieio'):
        e('std::atomic_thread_fence(std::memory_order_acq_rel);'); return S
    if mm_ in ('dcbt', 'dcbtst', 'dcbf', 'dcbst', 'icbi', 'isync', 'dcbi'):
        e(';'); return S
    if mm_ == 'dcbz':   # Cell cache line = 128 bytes
        e('{ uint32_t ea_ = %s & ~127u; std::memset(c.mem + ea_, 0, 128); }' % eax(ops[0], ops[1])); return S

    # ---- floating point arithmetic ----
    FS = {'fadds': '(double)(float)(%s + %s)', 'fsubs': '(double)(float)(%s - %s)', 'fmuls': '(double)(float)(%s * %s)',
          'fdivs': '(double)(float)(%s / %s)', 'fadd': '(%s + %s)', 'fsub': '(%s - %s)', 'fmul': '(%s * %s)', 'fdiv': '(%s / %s)'}
    if mm_ in FS:
        e('%s = %s;' % (F(ops[0]), FS[mm_] % (F(ops[1]), F(ops[2])))); return S
    if mm_ in ('fmadds', 'fmsubs', 'fnmadds', 'fnmsubs', 'fmadd', 'fmsub', 'fnmadd', 'fnmsub'):
        a_, c_, b_ = F(ops[1]), F(ops[2]), F(ops[3])
        core = {'fmadd': 'std::fma(%s, %s, %s)' % (a_, c_, b_), 'fmsub': 'std::fma(%s, %s, -%s)' % (a_, c_, b_),
                'fnmadd': '-std::fma(%s, %s, %s)' % (a_, c_, b_), 'fnmsub': '-std::fma(%s, %s, -%s)' % (a_, c_, b_)}[mm_.rstrip('s') if mm_.endswith('s') and mm_ != 'fmadd' else mm_] if False else None
        base = mm_[:-1] if mm_.endswith('s') else mm_
        core = {'fmadd': 'std::fma(%s, %s, %s)', 'fmsub': 'std::fma(%s, %s, -%s)',
                'fnmadd': '-std::fma(%s, %s, %s)', 'fnmsub': '-std::fma(%s, %s, -%s)'}[base] % (a_, c_, b_)
        e('%s = %s;' % (F(ops[0]), ('(double)(float)(%s)' % core) if mm_.endswith('s') else core)); return S
    if mm_ == 'fmr': e('%s = %s;' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'fneg': e('%s = -%s;' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'fabs': e('%s = std::fabs(%s);' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'fnabs': e('%s = -std::fabs(%s);' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'frsp': e('%s = (double)(float)%s;' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'fsqrts': e('%s = (double)(float)std::sqrt(%s);' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'fsqrt': e('%s = std::sqrt(%s);' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'fsel': e('%s = (%s >= 0.0) ? %s : %s;' % (F(ops[0]), F(ops[1]), F(ops[2]), F(ops[3]))); return S
    if mm_ == 'fctiwz': e('%s = c.fctiwz(%s);' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'fctidz': e('%s = c.fctidz(%s);' % (F(ops[0]), F(ops[1]))); return S
    if mm_ == 'fcfid': e('%s = (double)(int64_t)c.fbits(%s);' % (F(ops[0]), F(ops[1]))); return S
    if mm_ in ('fres',): e('%s = (double)(float)(1.0 / %s);' % (F(ops[0]), F(ops[1]))); return S
    if mm_ in ('frsqrte',): e('%s = 1.0 / std::sqrt(%s);' % (F(ops[0]), F(ops[1]))); return S

    # ---- VMX ----
    if mm_ in ('lvx', 'lvxl'): e('c.v[%s] = c.lvx(%s);' % (VREG.match(ops[0]).group(1), eax(ops[1], ops[2]))); return S
    if mm_ in ('stvx', 'stvxl'): e('c.stvx(%s, %s);' % (eax(ops[1], ops[2]), V(ops[0]))); return S
    if mm_ == 'lvewx': e('c.v[%s] = c.lvewx(%s);' % (VREG.match(ops[0]).group(1), eax(ops[1], ops[2]))); return S
    if mm_ == 'stvewx': e('c.stvewx(%s, %s);' % (eax(ops[1], ops[2]), V(ops[0]))); return S
    if mm_ == 'lvsl': e('c.v[%s] = c.lvsl(%s);' % (VREG.match(ops[0]).group(1), eax(ops[1], ops[2]))); return S
    if mm_ == 'lvsr': e('c.v[%s] = c.lvsr(%s);' % (VREG.match(ops[0]).group(1), eax(ops[1], ops[2]))); return S
    VOPS3 = ['vaddfp', 'vsubfp', 'vxor', 'vor', 'vand', 'vandc', 'vminfp', 'vmaxfp', 'vcmpeqfp', 'vcmpgefp', 'vcmpgtfp',
             'vcmpequw', 'vmrghw', 'vmrglw', 'vslw', 'vsraw', 'vsrw', 'vnor', 'vadduwm', 'vsubuwm']
    if mm_ in VOPS3:
        e('c.v[%s] = vx_%s(%s, %s);' % (VREG.match(ops[0]).group(1), mm_, V(ops[1]), V(ops[2])))
        if rec: e('c.set_crf(6, vx_cr6(c.v[%s]));' % VREG.match(ops[0]).group(1))
        return S
    if mm_ == 'vmr': e('%s = %s;' % (V(ops[0]), V(ops[1]))); return S
    if mm_ in ('vmaddfp', 'vnmsubfp'):
        e('c.v[%s] = vx_%s(%s, %s, %s);' % (VREG.match(ops[0]).group(1), mm_, V(ops[1]), V(ops[2]), V(ops[3]))); return S
    if mm_ in ('vperm', 'vsel'):
        e('c.v[%s] = vx_%s(%s, %s, %s);' % (VREG.match(ops[0]).group(1), mm_, V(ops[1]), V(ops[2]), V(ops[3]))); return S
    if mm_ == 'vsldoi': e('c.v[%s] = vx_vsldoi(%s, %s, %d);' % (VREG.match(ops[0]).group(1), V(ops[1]), V(ops[2]), num(ops[3]))); return S
    if mm_ == 'vspltw': e('c.v[%s] = vx_vspltw(%s, %d);' % (VREG.match(ops[0]).group(1), V(ops[1]), num(ops[2]))); return S
    if mm_ == 'vspltisw': e('c.v[%s] = vx_vspltisw(%d);' % (VREG.match(ops[0]).group(1), num(ops[1]))); return S
    if mm_ in ('vrsqrtefp', 'vrefp'): e('c.v[%s] = vx_%s(%s);' % (VREG.match(ops[0]).group(1), mm_, V(ops[1]))); return S
    if mm_ in ('vctsxs', 'vcfsx'): e('c.v[%s] = vx_%s(%s, %d);' % (VREG.match(ops[0]).group(1), mm_, V(ops[1]), num(ops[2]))); return S
    raise Unsupported('%s %s' % (m, o))


JT = set()
# exact switch tables (tools/jumptables.py): bctr address -> case targets
JTMAP = {}
_jp = os.path.join(RE, 'jt_map.txt')
if os.path.exists(_jp):
    for _l in open(_jp):
        _b, _t = _l.split(':')
        JTMAP[int(_b, 16)] = [int(x, 16) for x in _t.split()]


COND_B = ('beq', 'bne', 'blt', 'bgt', 'ble', 'bge', 'bdnz', 'bdz', 'bun', 'bnu')


def branch_target(m, o):
    """Target of a non-linking direct branch (b / bc), else None."""
    base = m.rstrip('+-')
    if base != 'b' and base not in COND_B: return None
    t = o.split(',')[-1].strip() if o else ''
    return int(t, 16) if t.startswith('0x') else None


def cfg_body(fa, fend):
    """Addresses of every instruction reachable from fa without leaving through a call or tail call.
    Functions are no longer cut at the next start address: their out-of-line blocks and loops stay together."""
    seen = set()
    stack = [fa]
    has_bctr = False
    while stack:
        a = stack.pop()
        while a in idx and a not in seen:
            if a != fa and a in STRONG: break          # running into another function: tail call
            seen.add(a)
            m, o = ins[idx[a]][1], ins[idx[a]][2]
            if m == '.byte': break
            if m == 'bctr':
                if a in JTMAP: stack.extend(t for t in JTMAP[a] if t in idx and (t == fa or t not in STRONG))
                else: has_bctr = True
            t = branch_target(m, o)
            if t is not None and t in idx and (t == fa or t not in STRONG): stack.append(t)
            if m in ('b', 'blr', 'bctr', 'ba'): break
            a += 4
        if not stack and has_bctr:
            # switch tables: case blocks are only reachable through the table; take the linear range as well
            has_bctr = False
            stack = [x for x in addrs[idx[fa]:bisect.bisect_left(addrs, fend)] if x not in seen]
    return sorted(seen)


def translate_function(fa, fend):
    order = cfg_body(fa, fend)
    present = set(order)
    body = [ins[idx[a]] for a in order]
    labels = set()
    for a, m, o in body:
        t = branch_target(m, o)
        if t is not None and t in present and (t == fa or t not in STRONG): labels.add(t)
    # fall-through into a block that is emitted elsewhere in address order
    for k, (a, m, o) in enumerate(body):
        nxt = body[k + 1][0] if k + 1 < len(body) else None
        if m not in ('b', 'blr', 'bctr', 'ba') and nxt != a + 4 and (a + 4) in present: labels.add(a + 4)
    # functions with an indirect jump (bctr): every block start after an unconditional branch may be a case target
    JT.clear()
    for a, m, o in body:
        if m == 'bctr' and a in JTMAP: JT.update(t for t in JTMAP[a] if t in present)
    if any(m == 'bctr' and a not in JTMAP for a, m, o in body):
        for k in range(1, len(body)):
            if body[k - 1][1] in ('b', 'blr', 'bctr') and body[k][0] == body[k - 1][0] + 4:
                JT.add(body[k][0])
    labels |= JT
    out = ['void f_%08x(PPU& c) {' % fa]
    # blocks are emitted in address order: when code below the entry belongs to the function, start at the entry
    if body and body[0][0] != fa:
        labels.add(fa)
        out.append('    goto L_%x;' % fa)
    unsupported = []
    for k, (a, m, o) in enumerate(body):
        if a in labels: out.append('L_%x:' % a)
        try:
            for st in tr_patched(a, m, o, fa, fend, labels):
                out.append('    ' + st)
        except (Unsupported, ValueError, AttributeError, KeyError, IndexError) as ex:
            unsupported.append('%x %s %s' % (a, m, o))
            out.append('    c.unimpl(0x%x); return; // %s %s' % (a, m, o))
            continue
        nxt = body[k + 1][0] if k + 1 < len(body) else None
        if m not in ('b', 'blr', 'bctr', 'ba') and nxt != a + 4:
            # sequential flow continues at a+4: elsewhere in this body, or in the next function
            out.append(('    goto L_%x;' % (a + 4)) if (a + 4) in present else ('    { rc_call(c, 0x%x); return; }' % (a + 4)))
    out.append('}')
    return out, unsupported


def main():
    out_path = sys.argv[1]
    ranges = [tuple(int(x, 16) for x in r.split('-')) for r in sys.argv[2:]]
    funcs = [f for f in starts if any(lo <= f < hi for lo, hi in ranges) and f in idx]
    code, decls, table, bad = [], [], [], []
    for f in funcs:
        k = bisect.bisect_right(starts, f)
        fend = starts[k] if k < len(starts) else f + 4
        lines, un = translate_function(f, fend)
        code += lines + ['']
        decls.append('void f_%08x(PPU& c);' % f)
        table.append('    {0x%x, f_%08x},' % (f, f))
        bad += un
    head = ('// generated by tools/ppc2cpp.py from the user\'s decrypted EBOOT - do not distribute\n'
            '#include "recomp/ppu.h"\n#include <cmath>\n\nnamespace gti::recomp {\n\n')
    tail_table = '\nconst FuncEntry kFuncs[] = {\n' + '\n'.join(table) + '\n};\nconst size_t kFuncCount = %d;\n\n} // namespace gti::recomp\n' % len(table)
    parts = int(os.environ.get('SPLIT', '1'))
    if parts <= 1:
        with open(out_path, 'w') as fh:
            fh.write(head + '\n'.join(decls) + '\n\n' + '\n'.join(code) + tail_table)
    else:
        # <base>_NN.cpp hold the function bodies, <base>_table.cpp the dispatch table
        base = out_path[:-4] if out_path.endswith('.cpp') else out_path
        blocks, cur = [], []
        for line in code:
            if line.startswith('void f_') and cur:
                blocks.append(cur); cur = []
            cur.append(line)
        if cur: blocks.append(cur)
        per = (len(blocks) + parts - 1) // parts
        for k in range(parts):
            with open('%s_%02d.cpp' % (base, k), 'w') as fh:
                body = [l for b in blocks[k * per:(k + 1) * per] for l in b]
                fh.write(head + '\n'.join(decls) + '\n\n' + '\n'.join(body) + '\n\n} // namespace gti::recomp\n')
        with open('%s_table.cpp' % base, 'w') as fh:
            fh.write(head + '\n'.join(decls) + '\n' + tail_table)
    print('functions', len(funcs), 'unsupported instructions', len(bad))
    for b in bad[:30]: print('  ', b)


if __name__ == '__main__':
    main()
