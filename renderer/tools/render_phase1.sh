#!/usr/bin/env bash
# Phase 1 proof frames. Renders to $OUT (raw HDR + AOVs), then post-processes
# to docs/phase1/*.jpg. Times are GST (UTC+4) on 21 June 2026.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/zawal-phase1}
DOCS=../docs/phase1
mkdir -p "$OUT" "$DOCS"
make -s
r() { # name scene cam time w h spp ev [extra...]
  local name=$1 scene=$2 cam=$3 t=$4 w=$5 h=$6 spp=$7 ev=$8; shift 8
  if [ ! -f "$OUT/$name/color.f32" ]; then
    ./bin/zawal --scene "$scene" --cam "$cam" --time "$t" --w "$w" --h "$h" --spp "$spp" --out "$OUT/$name" "$@"
  fi
  python3 tools/post.py "$OUT/$name" "$DOCS/$name.jpg" --ev "$ev" --quality 90
  cp "$OUT/$name/meta.json" "$DOCS/$name.json"
}
r court_0800      house court   08:00    1600 900 64 1.2
r court_zawal     house court   12:20:43 1600 900 64 0.4
r court_1700      house court   17:00    1600 900 64 1.3
r street_0730     house street  07:30    1600 900 64 1.0
r qudra_1340      qudra qudra   13:40    1600 900 40 0.5 --lat 24.84 --lon 55.37
r court_zawal_p   house court_p 12:20:43 900 1600 48 0.4 --vfov 88 --shift 0.16
r street_0730_p   house street_p 07:30   900 1600 48 1.0 --vfov 84 --shift 0.22
echo "phase 1 frames done"
