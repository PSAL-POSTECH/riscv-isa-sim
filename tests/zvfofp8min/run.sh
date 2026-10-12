#!/usr/bin/env bash
# Builds target.c/traps.c, runs the target under spike, checks it with the exact oracle, then the
# negative controls: gating without zvfofp8min, SEW=8-only encodings, and altfmt legality in vtype.
set -uo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SPIKE=${SPIKE:-"$HERE/../../build/spike"}
PK=${PK:-$(find /opt/vcix-env/pk -name pk -type f | head -1)}
CROSS=${CROSS:-/opt/vcix-env/riscv/bin/riscv64-unknown-elf-gcc}
ISA=${ISA:-rv64gcv_zvfofp8min_zvfbfmin}
OUT=${OUT:-"$HERE/out.bin"}
CF="-O2 -march=rv64gcv -mabi=lp64d"
rc=0
$CROSS $CF -o "$HERE/target" "$HERE/target.c" || exit 2
"$SPIKE" --isa="$ISA" "$PK" "$HERE/target" > "$OUT" || { echo "target failed under $ISA"; exit 2; }
python3 "$HERE/oracle.py" "$OUT" || rc=1
MUT=$(python3 "$HERE/oracle.py" "$OUT" --mutate 123457)
if [[ "$MUT" == *"mismatches: 1" ]]; then echo "ok   oracle catches a mutated record"
else echo "BAD  oracle missed a mutated record"; rc=1; fi
# name : T : VTYPE : isa : expect (run|trap|vill|legal)
CASES="
vfncvt.f.f.q@no_ext:0:0:rv64gcv_zvfbfmin:trap
vfncvt.sat.f.f.q@no_ext:1:0:rv64gcv_zvfbfmin:trap
vfncvtbf16.f.f.w@e8_no_ext:2:0:rv64gcv_zvfbfmin:trap
vfncvtbf16.sat.f.f.w@no_ext:3:0:rv64gcv_zvfbfmin:trap
vfwcvtbf16.f.f.v@e8_no_ext:4:0:rv64gcv_zvfbfmin:trap
vfncvt.f.f.q@ctl:0:0:$ISA:run
vfncvt.sat.f.f.q@ctl:1:0:$ISA:run
vfncvtbf16.f.f.w@e8_ctl:2:0:$ISA:run
vfncvtbf16.sat.f.f.w@ctl:3:0:$ISA:run
vfwcvtbf16.f.f.v@e8_ctl:4:0:$ISA:run
vfncvt.f.f.q@ctl_only_ofp8:0:0:rv64gcv_zvfofp8min:run
vfwcvtbf16.f.f.v@e8_only_ofp8:4:0:rv64gcv_zvfofp8min:run
vfncvt.sat.f.f.q@e16:5:0:$ISA:trap
vfncvtbf16.sat.f.f.w@e16:6:0:$ISA:trap
vfncvt.f.f.q@e16:7:0:$ISA:trap
altfmt@e8:10:0x1c0:$ISA:legal
altfmt@e8_zvfp8:10:0x1c0:rv64gcv_zvfp8:legal
altfmt@e8_no_ext:10:0x1c0:rv64gcv_zvfbfmin:vill
altfmt@e16:11:0x1c8:$ISA:vill
altfmt@e32:12:0x1d0:$ISA:vill
altfmt@e64:13:0x1d8:$ISA:vill
altfmt0@e16:14:0x0c8:$ISA:legal
"
for c in $CASES; do
  IFS=: read -r nm t vt isa want <<< "$c"
  $CROSS $CF -DT=$t -DVTYPE=$vt -o "$HERE/trap_bin" "$HERE/traps.c" || exit 2
  got=$("$SPIKE" --isa="$isa" "$PK" "$HERE/trap_bin" 2>&1)
  if [[ "$got" != *AFTER* ]]; then have=trap
  elif (( t >= 10 )); then [[ "$got" == *"vtype=8000000000000000"* ]] && have=vill || have=legal
  else have=run; fi
  if [ "$have" = "$want" ]; then printf "ok   %-34s %-30s %s\n" "$nm" "$isa" "$have"
  else printf "BAD  %-34s %-30s want=%s got=%s\n" "$nm" "$isa" "$want" "$have"; rc=1; fi
done
rm -f "$HERE/trap_bin"
[ $rc -eq 0 ] && echo "run.sh: PASS" || echo "run.sh: FAIL"
exit $rc
