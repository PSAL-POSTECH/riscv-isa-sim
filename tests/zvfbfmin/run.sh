#!/usr/bin/env bash
# Builds target.c, runs it under spike with and without zvfbfmin, and checks the output with oracle.py.
set -uo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SPIKE=${SPIKE:-"$HERE/../../build/spike"}
PK=${PK:-/workspace/riscv-pk/build/pk}
CROSS=${CROSS:-/workspace/riscv/bin/riscv64-unknown-elf-gcc}
VARCH=${VARCH:-vlen:512,elen:64}
$CROSS -O1 -march=rv64gcv -mabi=lp64d -o "$HERE/target" "$HERE/target.c" || exit 2
"$SPIKE" --isa=rv64gcv_zvfbfmin --varch="$VARCH" "$PK" "$HERE/target" > "$HERE/out.txt" || exit 2
python3 "$HERE/oracle.py" "$HERE/out.txt" || exit 1
NEG=$("$SPIKE" --isa=rv64gcv --varch="$VARCH" "$PK" "$HERE/target" 2>&1)
if [[ "$NEG" == *"illegal instruction"* ]]; then
  echo "negative control: illegal instruction without zvfbfmin"
else
  echo "negative control FAILED: ran without zvfbfmin"; exit 1
fi
