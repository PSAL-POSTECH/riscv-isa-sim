#!/usr/bin/env bash
# What this fork adds to Spike: --machine-config, the scratchpad, the refusal of an
# extension that overlaps an implemented encoding, and the extension vcixaccel.
# Usage: ci-tests/vcix/run.sh <spike build directory> <pk>
# Needs gcc, g++ and a RISC-V gcc (RISCV_CC, default riscv64-unknown-elf-gcc).
set -uo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
BUILD=$(cd "$1" && pwd)
PK=$2
SPIKE="$BUILD/spike"
OUT="$BUILD/ci-vcix"
RISCV_CC=${RISCV_CC:-riscv64-unknown-elf-gcc}
LOG="$OUT/last.log"
failed=0
mkdir -p "$OUT"

# program <name> <source> [-D...]: $OUT/<name>, "${RUN[@]}" under pk.
program() {
  "$RISCV_CC" -march=rv64gcv -static "${@:3}" "$HERE/$2" -o "$OUT/$1" || { echo "FAIL  $1 does not build"; exit 2; }
}
# model <name> [-D...]: $OUT/lib<name>.so from model.c, against this tree's copy of the interface.
model() {
  gcc -shared -fPIC -fvisibility=hidden -I"$SRC/riscv" "${@:2}" "$HERE/model.c" -o "$OUT/lib$1.so" \
    || { echo "FAIL  model $1 does not build"; exit 2; }
}
# claims <name> [-D...]: $OUT/lib<name>.so from claims.cc. extension.cc is compiled in: nothing in
# spike refers to it, so it is not in the binary.
claims() {
  g++ -std=c++11 -shared -fPIC -I"$SRC/riscv" -I"$SRC/softfloat" -I"$SRC/fesvr" -I"$SRC" -I"$BUILD" \
    "${@:2}" "$HERE/claims.cc" "$SRC/riscv/extension.cc" -o "$OUT/lib$1.so" \
    || { echo "FAIL  extension $1 does not build"; exit 2; }
}
# yml <name> <line>...: $OUT/<name>.yml
yml() { printf '%s\n' "${@:2}" > "$OUT/$1.yml"; }

# expect <what> <exit code, or nonzero> <text the output must contain, or ""> <command...>
expect() {
  local what=$1 want=$2 text=$3 rc ok=1
  shift 3
  timeout 120 "$@" < /dev/null > "$LOG" 2>&1; rc=$?
  if [ "$want" = nonzero ]; then [ "$rc" != 0 ] || ok=0; else [ "$rc" = "$want" ] || ok=0; fi
  [ -z "$text" ] || grep -Fq -- "$text" "$LOG" || ok=0
  if [ $ok = 1 ]; then
    echo "PASS  $what (exit $rc)"
  else
    echo "FAIL  $what (exit $rc, want $want${text:+ and \"$text\"})"
    tail -n 5 "$LOG" | sed 's/^/      /'
    failed=1
  fi
}
# count <what> <want> <text>: how many lines of the last run's output contain <text>.
count() {
  local got; got=$(grep -Fc -- "$3" "$LOG")
  if [ "$got" = "$2" ]; then echo "PASS  $1 ($got of $2)"; else echo "FAIL  $1 ($got of $2)"; failed=1; fi
}

BASE=0xD0000000        # where the scratchpad is unless something says otherwise
MOVED=0x0A000000
LANE_KB=128
program vlenb vlenb.S
program spad_4 spad.S -DSPAD_BASE=$BASE -DSPAD_SIZE=0x80000
program spad_8 spad.S -DSPAD_BASE=$BASE -DSPAD_SIZE=0x100000
program spad_4_past spad.S -DSPAD_BASE=$BASE -DSPAD_SIZE=0x80000 -DOFFSET=4
program spad_4_next spad.S -DSPAD_BASE=$BASE -DSPAD_SIZE=0x80000 -DOFFSET=8
program spad_8_moved spad.S -DSPAD_BASE=$MOVED -DSPAD_SIZE=0x100000
program custom2 custom2.S -DVSET
program custom2_vill custom2.S

GOOD=("vpu_num_lanes: 8" "vpu_spad_size_kb_per_lane: $LANE_KB" "vpu_vector_length_bits: 256")
yml good "vpu_num_lanes: 8" "vpu_spad_size_kb_per_lane: $LANE_KB" "vpu_vector_length_bits: 256" "unrelated: text"
yml moved "vpu_num_lanes: 8" "vpu_spad_size_kb_per_lane: $LANE_KB" "vpu_vector_length_bits: 256" "vpu_spad_base_vaddr: $MOVED"
# The commands under test. They are spelled out, not shell functions: `timeout` runs a program.
RUN=("$SPIKE" --isa=rv64gcv)
ACCEL=("$SPIKE" --isa=rv64gcv_xvcixaccel)
config() { echo "--machine-config=$OUT/$1.yml"; }

echo "-- the scratchpad, from options"
expect "4 lanes of ${LANE_KB}KB at $BASE by default: first and last doubleword" 0 "" "${RUN[@]}" "$PK" "$OUT/spad_4"
expect "an access reaching 4 bytes past the end is not the scratchpad's" nonzero "" "${RUN[@]}" "$PK" "$OUT/spad_4_past"
expect "an access just after the end is not the scratchpad's" nonzero "" "${RUN[@]}" "$PK" "$OUT/spad_4_next"
expect "the last doubleword of 8 lanes is outside 4" nonzero "" "${RUN[@]}" "$PK" "$OUT/spad_8"
expect "--vectorlane-size=8 makes it 8 lanes" 0 "" "${RUN[@]}" --vectorlane-size=8 "$PK" "$OUT/spad_8"
expect "--scratchpad-base-vaddr moves it" 0 "" "${RUN[@]}" --vectorlane-size=8 --scratchpad-base-vaddr=$((MOVED)) "$PK" "$OUT/spad_8_moved"
expect "--scratchpad-base-paddr is gone" nonzero "unrecognized option" "${RUN[@]}" --scratchpad-base-paddr=3221225472 "$PK" "$OUT/spad_4"
expect "SPIKE_DEBUG=1 without --base-path runs" 16 "" env SPIKE_DEBUG=1 "$SPIKE" --isa=rv64gcv "$PK" "$OUT/vlenb"

echo "-- the machine description"
expect "VLEN is 128 without one" 16 "" "${RUN[@]}" "$PK" "$OUT/vlenb"
expect "vpu_vector_length_bits: 256 is VLEN" 32 "" "${RUN[@]}" "$(config good)" "$PK" "$OUT/vlenb"
expect "vpu_num_lanes: 8 and vpu_spad_size_kb_per_lane size the scratchpad" 0 "" "${RUN[@]}" "$(config good)" "$PK" "$OUT/spad_8"
expect "vpu_spad_base_vaddr moves the scratchpad" 0 "" "${RUN[@]}" "$(config moved)" "$PK" "$OUT/spad_8_moved"
expect "and nothing is left at $BASE" nonzero "" "${RUN[@]}" "$(config moved)" "$PK" "$OUT/spad_8"

for option in --varch=vlen:256,elen:64 --vectorlane-size=8 --scratchpad-size=131072 --scratchpad-base-vaddr=$((MOVED)); do
  name=${option%%=*}
  expect "refused beside $name, given after" 1 "it sets what $name sets" "${RUN[@]}" "$(config good)" "$option" "$PK" "$OUT/vlenb"
  expect "refused beside $name, given before" 1 "it sets what $name sets" "${RUN[@]}" "$option" "$(config good)" "$PK" "$OUT/vlenb"
done

for missing in 0 1 2; do
  lines=("${GOOD[@]}"); key=${lines[$missing]%%:*}; unset "lines[$missing]"
  yml missing "${lines[@]}"
  expect "refused without $key" 1 "no value for $key" "${RUN[@]}" "$(config missing)" "$PK" "$OUT/vlenb"
done
# value|why it is refused
for case in '~|no value for vpu_num_lanes' 'abc|vpu_num_lanes is not an unsigned number' '-1|vpu_num_lanes is not an unsigned number' \
            '1.5|vpu_num_lanes is not an unsigned number' '[8]|vpu_num_lanes is not an unsigned number' \
            '4294967296|vpu_num_lanes does not fit in 32 bits'; do
  yml lanes "vpu_num_lanes: ${case%%|*}" "${GOOD[@]:1}"
  expect "refused with vpu_num_lanes: ${case%%|*}" 1 "${case#*|}" "${RUN[@]}" "$(config lanes)" "$PK" "$OUT/vlenb"
done
yml vlen "${GOOD[@]:0:2}" "vpu_vector_length_bits: 100"
expect "refused with vpu_vector_length_bits: 100" 1 "vpu_vector_length_bits is not a power of two from 64 to 4096" "${RUN[@]}" "$(config vlen)" "$PK" "$OUT/vlenb"
yml huge "vpu_num_lanes: 8" "vpu_spad_size_kb_per_lane: 18014398509481984" "vpu_vector_length_bits: 256"
expect "refused when the scratchpad's size does not fit" 1 "does not fit in 64 bits as bytes" "${RUN[@]}" "$(config huge)" "$PK" "$OUT/vlenb"
yml reserved "${GOOD[@]}" "run_base_path: /somewhere"
expect "refused with run_base_path in the file" 1 "run_base_path is the key --base-path is handed on under" "${RUN[@]}" "$(config reserved)" "$PK" "$OUT/vlenb"
yml sequence "- vpu_num_lanes: 8"
expect "refused when the top level is a sequence" 1 "the top level of a machine description is a mapping" "${RUN[@]}" "$(config sequence)" "$PK" "$OUT/vlenb"
yml empty "# nothing"
expect "refused when there is no document" 1 "no value for vpu_num_lanes" "${RUN[@]}" "$(config empty)" "$PK" "$OUT/vlenb"
yml broken "vpu_num_lanes: [8"
expect "refused when it is not YAML" 1 "--machine-config=$OUT/broken.yml: " "${RUN[@]}" "$(config broken)" "$PK" "$OUT/vlenb"
expect "refused when it cannot be read" 1 "--machine-config=$OUT/no_such_file.yml: " "${RUN[@]}" "$(config no_such_file)" "$PK" "$OUT/vlenb"

echo "-- overlapping encodings"
claims overlaps
claims free -DFREE
expect "an extension claiming add is refused, named in the ISA string" nonzero "overlaps an implemented instruction" \
  "$SPIKE" --extlib="$OUT/liboverlaps.so" --isa=rv64gcv_xclaims "$PK" "$OUT/vlenb"
expect "an extension claiming add is refused, given with --extension" nonzero "overlaps an implemented instruction" \
  "$SPIKE" --extlib="$OUT/liboverlaps.so" --extension=claims --isa=rv64gcv "$PK" "$OUT/vlenb"
expect "one claiming a free encoding runs, named in the ISA string" 16 "" \
  "$SPIKE" --extlib="$OUT/libfree.so" --isa=rv64gcv_xclaims "$PK" "$OUT/vlenb"
expect "one claiming a free encoding runs, given with --extension" 16 "" \
  "$SPIKE" --extlib="$OUT/libfree.so" --extension=claims --isa=rv64gcv "$PK" "$OUT/vlenb"

echo "-- the extension vcixaccel"
model model
lib() { echo "--extlib=$OUT/lib$1.so"; }

expect "custom-2 is illegal without the extension" nonzero "An illegal instruction was executed!" "${RUN[@]}" "$PK" "$OUT/custom2"
expect "the extension without a model is refused" 1 "vcixaccel: no accelerator model is loaded" \
  "${ACCEL[@]}" "$PK" "$OUT/custom2"
expect "the model executes the instruction, on the default machine" 0 "[model] execute 062541db lanes 4 vlen 128" "${ACCEL[@]}" "$(lib model)" "$PK" "$OUT/custom2"
count  "once" 1 "[model] execute"
count  "and is configured with nothing, without a machine description" 4 " is absent"
expect "the model sees the machine --machine-config describes" 0 "[model] execute 062541db lanes 8 vlen 256" \
  "${ACCEL[@]}" "$(lib model)" "$(config good)" "$PK" "$OUT/custom2"
count  "is configured from the text of the file" 1 "[model] vpu_num_lanes = <8>"
count  "a key the file does not have is absent" 1 "[model] vpu_spad_base_vaddr is absent"
count  "and so is run_base_path without --base-path" 1 "[model] run_base_path is absent"
expect "vpu_spad_base_vaddr reaches the model as written" 0 "[model] vpu_spad_base_vaddr = <$MOVED>" \
  "${ACCEL[@]}" "$(lib model)" "$(config moved)" "$PK" "$OUT/custom2"
expect "--base-path reaches the model as run_base_path" 0 "[model] run_base_path = <$OUT>" \
  "${ACCEL[@]}" "$(lib model)" "$(config good)" --base-path="$OUT" "$PK" "$OUT/custom2"
expect "and does without a machine description" 0 "[model] run_base_path = <$OUT>" "${ACCEL[@]}" "$(lib model)" --base-path="$OUT" "$PK" "$OUT/custom2"
expect "two harts" 0 "" "${ACCEL[@]}" "$(lib model)" -p2 "$PK" "$OUT/custom2"
count  "make two instances" 2 "[model] vpu_num_lanes"
expect "custom-2 before any vsetvli is illegal" nonzero "An illegal instruction was executed!" "${ACCEL[@]}" "$(lib model)" "$PK" "$OUT/custom2_vill"
count  "and does not reach the model" 0 "[model] execute"

# name|what is wrong|what Spike says
for case in 'no_table|-DNO_TABLE|vcix_accel_model() returned no table' \
            'other_abi|-DABI=VCIX_ACCEL_ABI_VERSION+1|, Spike has ' \
            'no_execute|-DNO_EXECUTE|fork_test: create, destroy or execute is NULL' \
            'custom0|-DOPCODE=0x0b|is not in custom-1 or custom-2' \
            'bad_lane|-DBAD_LANE|vcixaccel: fork_test: asked for lane 4 of 4' \
            'bad_register|-DBAD_REGISTER|vcixaccel: fork_test: asked for x register 32 of 32'; do
  IFS='|' read -r name define says <<< "$case"
  model "$name" "$define"
  expect "a model built with $define is refused" 1 "$says" "${ACCEL[@]}" "$(lib "$name")" "$PK" "$OUT/custom2"
done

echo
if [ $failed = 0 ]; then echo "all passed"; else echo "FAILED"; fi
exit $failed
