# Independent IEEE reference for tests/zvfbfmin: exact rational rounding per frm, tininess after rounding,
# compared element by element and flag set by flag set against the spike output given as argv[1].
from fractions import Fraction as F
import math, re, sys
f32in=[0x3f800000,0x3f808000,0x3f818000,0x3f80c000,0x3f804000,0xbf808001,0x7f7fffff,0x7f800000,0xff800000,
 0x7fc00000,0x7f800001,0x00000001,0x00008000,0x00018000,0x80000000,0x00000000,0x3eaaaaab,0x477fe000,0x40490fdb,0xc2c80000]
bfin=[0x3f80,0x7f80,0xff80,0x7fc0,0x7f81,0x0001,0x8001,0x0000,0x8000,0x4049,0xc2c8,0x7f7f]
NX,UF,OF,NV=1,2,4,16
def f32val(b):
    s=-1 if b>>31 else 1; e=(b>>23)&0xff; m=b&0x7fffff
    if e==0: return s*F(m,1<<23)*F(1,2**126)
    return s*(1+F(m,1<<23))*(F(2)**(e-127))
def rnd(v,mode,step):
    q=v/step; fl=math.floor(q); r=q-fl
    if r==0: return fl*step
    if mode==0: n=fl+1 if (r>F(1,2) or (r==F(1,2) and fl%2)) else fl
    elif mode==1: n=fl if v>0 else fl+1
    elif mode==2: n=fl
    elif mode==3: n=fl+1
    else: n=fl+1 if r>=F(1,2) else fl
    if mode==4 and v<0: n=fl+1 if r>F(1,2) else fl   # ties away from zero for negatives
    return n*step
def encbf(v,neg):
    if v==0: return 0x8000 if neg else 0
    a=abs(v); e=math.floor(math.log2(a))
    while F(2)**e>a: e-=1
    while F(2)**(e+1)<=a: e+=1
    s=0x8000 if v<0 else 0
    if e<-126: return s|int(a/(F(2)**-133))
    return s|((e+127)<<7)|int((a/(F(2)**e)-1)*128)
MAX=(2-F(1,128))*F(2)**127
def narrow(b,mode):
    e=(b>>23)&0xff; m=b&0x7fffff; neg=b>>31
    if e==0xff and m: return 0x7fc0,(NV if not (m>>22)&1 else 0)
    if e==0xff: return (0xff80 if neg else 0x7f80),0
    v=f32val(b)
    if v==0: return (0x8000 if neg else 0),0
    ex=math.floor(math.log2(abs(v)))
    while F(2)**ex>abs(v): ex-=1
    while F(2)**(ex+1)<=abs(v): ex+=1
    unb=rnd(v,mode,F(2)**(ex-7))           # unbounded exponent rounding
    r=rnd(v,mode,F(2)**(max(ex,-126)-7))
    fl=0
    if r!=v: fl|=NX
    if abs(unb)>MAX:
        fl|=OF|NX
        toinf = mode in (0,4) or (mode==3 and v>0) or (mode==2 and v<0)
        return ((0xff80 if neg else 0x7f80) if toinf else (0xff7f if neg else 0x7f7f)), fl
    if (fl&NX) and abs(unb)<F(2)**-126: fl|=UF
    return encbf(r,neg),fl
def widen(h):
    e=(h>>7)&0xff; m=h&0x7f
    if e==0xff and m: return 0x7fc00000,(NV if not (m>>6)&1 else 0)
    return h<<16,0
got={}
for line in open(sys.argv[1]):
    mm=re.match(r'(N frm=(\d)|W) fflags=(\w+): (.*)',line)
    if mm: got[mm.group(1)]=(int(mm.group(3),16),[int(x,16) for x in mm.group(4).split()])
bad=0
for mode in range(5):
    exp=[narrow(b,mode) for b in f32in]; fl=0
    for i,(v,f) in enumerate(exp):
        fl|=f
        if got[f'N frm={mode}'][1][i]!=v: bad+=1; print('MISMATCH N',mode,hex(f32in[i]),hex(v),hex(got[f'N frm={mode}'][1][i]))
    if got[f'N frm={mode}'][0]!=fl: bad+=1; print('FLAGS N',mode,hex(fl),hex(got[f'N frm={mode}'][0]))
exp=[widen(h) for h in bfin]; fl=0
for i,(v,f) in enumerate(exp):
    fl|=f
    if got['W'][1][i]!=v: bad+=1; print('MISMATCH W',hex(bfin[i]),hex(v),hex(got['W'][1][i]))
if got['W'][0]!=fl: bad+=1; print('FLAGS W',hex(fl),hex(got['W'][0]))
print('checked', 5*len(f32in)+len(bfin), 'values + 6 flag sets; mismatches:', bad)
sys.exit(1 if bad else 0)
