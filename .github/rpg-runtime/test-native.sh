#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
output=${1:?test output directory is required}
cc -std=gnu99 -DINLINE=inline -DRETROM_MAME_PLUS -DHAVE_ZLIB -DWANT_ZLIB \
  -DHAS_M6502=1 -DHAS_M6809=1 -DHAS_M68705=1 -DHAS_YM3526=1 -DHAS_ADPCM=1 \
  -ffunction-sections -fdata-sections \
  -I"$root/src" -I"$root/src/mame2003" -I"$root/src/libretro-common/include" \
  "$root/tests/renegade-checkpoint.c" "$root/src/state.c" \
  -Wl,--gc-sections -lz -lm -o "$output/renegade-checkpoint"
"$output/renegade-checkpoint"
