#!/bin/bash
# Build and run the multi-precision array end-to-end test under pk.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SPIKE=${SPIKE:-$HERE/../../../spike-install/bin/spike}
PK=${PK:-/opt/torchsim-local/toolchain/pk}
GCC=${GCC:-/opt/torchsim-local/toolchain/riscv/bin/riscv64-unknown-elf-gcc}
NM=${NM:-/opt/torchsim-local/toolchain/riscv/bin/riscv64-unknown-elf-nm}
LANES=${1:-32}
ELF=$HERE/msa_matmul.elf

$GCC -O2 -march=rv64gcv -mabi=lp64d "$HERE/msa_matmul.c" -o "$ELF"

# --kernel-addr is mandatory: outside it spike runs the vector unit with one lane.
read -r LO SZ < <($NM -S "$ELF" | awk '$4=="npu_kernel"{print $1, $2}')
HI=$(printf '%x' $((0x$LO + 0x$SZ)))

exec $SPIKE --isa=rv64gcv_zfh_zvfp8 --varch=vlen:256,elen:64 \
  --vectorlane-size=$LANES \
  -m0x80000000:0x80000000,0x70000000:0x$(printf '%x' $((65536 * LANES))) \
  --scratchpad-base-paddr=1879048192 --scratchpad-base-vaddr=3489660928 \
  --scratchpad-size=65536 --kernel-addr=$LO:$HI --base-path="$HERE" \
  "$PK" "$ELF" "$LANES"
