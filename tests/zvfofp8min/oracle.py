# Independent exact-integer oracle for tests/zvfofp8min, written from the Zvfofp8min v0.9 text and IEEE 754:
# rounding by frm at precision p, tininess after rounding, overflow judged with unbounded exponent; infinite
# results saturate (.sat), else E4M3 -> canonical NaN 0x7F, E5M2 -> signed inf; NaNs -> 0x7F; fp8->bf16 exact, no flags.
import struct, sys
from collections import Counter
NX, UF, OF, DZ, NV = 1, 2, 4, 8, 16
RNE, RTZ, RDN, RUP, RMM = range(5)
FMT = {0: dict(name='E4M3', mb=3, bias=7, emin=-6, maxn=7, maxq=6, enc_max=0x7E),
       1: dict(name='E5M2', mb=2, bias=15, emin=-14, maxn=7, maxq=13, enc_max=0x7B)}

def decode(bits, eb, mb):
    sign = bits >> (eb + mb); e = (bits >> mb) & ((1 << eb) - 1); m = bits & ((1 << mb) - 1)
    bias = (1 << (eb - 1)) - 1
    if e == (1 << eb) - 1:
        return sign, ('nan', not (m >> (mb - 1)) & 1) if m else ('inf', None)
    if e == 0: return sign, ('num', (m, 1 - bias - mb))
    return sign, ('num', ((1 << mb) | m, e - bias - mb))

def rnd(M, E, q, neg, mode):
    if E >= q: return M << (E - q), 0
    s = q - E; fl = M >> s; rem = M - (fl << s); half = 1 << (s - 1)
    up = {RNE: rem > half or (rem == half and fl & 1), RTZ: False, RDN: neg and rem > 0,
          RUP: (not neg) and rem > 0, RMM: rem >= half}[mode]
    return fl + bool(up), rem

def narrow(sign, kind, fmt, sat, mode):
    f = FMT[fmt]; p = f['mb'] + 1; s7 = sign << 7
    inf_res = s7 | f['enc_max'] if sat else (0x7F if fmt == 0 else s7 | 0x7C)
    if kind[0] == 'nan': return 0x7F, NV if kind[1] else 0
    if kind[0] == 'inf': return inf_res, 0
    M, E = kind[1]
    if M == 0: return s7, 0
    e = M.bit_length() - 1 + E
    q = max(e, f['emin']) - (p - 1)
    n, rem = rnd(M, E, q, sign, mode)
    if gt(n, q, f['maxn'], f['maxq']):
        to_inf = mode in (RNE, RMM) or (mode == RUP and not sign) or (mode == RDN and sign)
        return (inf_res if to_inf else s7 | f['enc_max']), OF | NX
    fl = NX if rem else 0
    nu, _ = rnd(M, E, e - (p - 1), sign, mode)
    if fl and lt_pow2(nu, e - (p - 1), f['emin']): fl |= UF
    return s7 | encode(n, q, f), fl

def gt(n, q, mn, mq):
    a, b = n, mn
    if q >= mq: a <<= q - mq
    else: b <<= mq - q
    return a > b

def lt_pow2(n, q, emin):
    return n.bit_length() - 1 + q < emin if n else True

def encode(n, q, f):
    if n == 0: return 0
    e = n.bit_length() - 1 + q; mb = f['mb']
    if e < f['emin']:
        sh = q - (f['emin'] - mb); assert sh >= 0; return n << sh
    sh = q - (e - mb); m = n << sh if sh >= 0 else n >> -sh
    assert sh >= 0 or (n & ((1 << -sh) - 1)) == 0
    return ((e + f['bias']) << mb) | (m - (1 << mb))

def widen(b, fmt):
    sign = b >> 7
    if fmt == 0:
        if b & 0x7F == 0x7F: return 0x7FC0, 0
        e, m = (b >> 3) & 15, b & 7
        kind = ('num', ((8 | m, e - 10) if e else (m, -9)))
    else:
        s, kind = decode(b, 5, 2)
        if kind[0] == 'nan': return 0x7FC0, 0
        if kind[0] == 'inf': return (sign << 15) | 0x7F80, 0
    M, E = kind[1]
    if M == 0: return sign << 15, 0
    e = M.bit_length() - 1 + E; sh = (e - 7) - E
    m = M >> sh if sh >= 0 else M << -sh
    return (sign << 15) | ((e + 127) << 7) | (m - 128), 0

def expect(op, mode, x):
    fmt, sat, frm = mode & 1, (mode >> 1) & 1, (mode >> 2) & 7
    if op in (0, 1): return narrow(*decode(x, 8, 23), fmt, op & 1, frm)
    if op in (2, 3): return narrow(*decode(x, 8, 7), fmt, op & 1, frm)
    return widen(x, fmt)

def main():
    args = sys.argv[1:]; mutate = None
    if '--mutate' in args: i = args.index('--mutate'); mutate = int(args[i + 1]); del args[i:i + 2]
    data = open(args[0], 'rb').read(); nrec = len(data) // 16
    bad = 0; chunks = {}; cov = Counter(); opn = ['vfncvt.f.f.q', 'vfncvt.sat.f.f.q', 'vfncvtbf16.f.f.w',
                                                 'vfncvtbf16.sat.f.f.w', 'vfwcvtbf16.f.f.v']
    for k, (op, mode, flags, vec, chunk, x, out) in enumerate(struct.iter_unpack('<BBBBIII', data)):
        if op == 255: print(f'vlenb={chunk}'); continue
        ev, ef = expect(op, mode, x)
        if k == mutate: ev ^= 1
        tag = f"{opn[op]} {FMT[mode & 1]['name']} frm={(mode >> 2) & 7}"
        if out != ev:
            bad += 1
            if bad <= 30: print(f'VALUE {tag} vec={vec} in={x:#x} want={ev:#x} got={out:#x}')
        if vec:
            c = chunks.setdefault(chunk, [0, flags, tag]); c[0] |= ef
        else:
            cov[(op, ef)] += 1
            if flags != ef:
                bad += 1
                if bad <= 30: print(f'FLAGS {tag} in={x:#x} want={ef:#x} got={flags:#x}')
    for cid, (ef, got, tag) in chunks.items():
        if ef != got:
            bad += 1
            if bad <= 30: print(f'CHUNKFLAGS {tag} chunk={cid} want={ef:#x} got={got:#x}')
    for op in range(5):
        print(f'  {opn[op]:22s} ' + ' '.join(f'{fl:#04x}:{n}' for (o, fl), n in sorted(cov.items()) if o == op))
    print(f'checked {nrec} records ({len(chunks)} vector chunks); mismatches: {bad}')
    return 1 if bad else 0

if __name__ == '__main__': sys.exit(main())
