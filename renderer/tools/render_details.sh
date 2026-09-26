#!/usr/bin/env bash
# Close-up detail renders for the realism pass: the things a full frame can
# show only at 100% (a crack, a lantern's shadow, a caustic). Same scene, same
# sun, cameras placed like a photographer walking the house. Resumable like
# render_phase1.sh: a detail whose color.f32 exists is not re-rendered.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/zawal-details}
DOCS=${DOCS:-../docs/realism/details}
mkdir -p "$OUT" "$DOCS"
make -s
d() { # name cam time ev campos camtgt hfov [extra...]
  local name=$1 cam=$2 t=$3 ev=$4 pos=$5 tgt=$6 fov=$7; shift 7
  if [ ! -f "$OUT/$name/color.f32" ]; then
    ./bin/zawal --scene house --cam "$cam" --time "$t" --w 800 --h 600 --spp 64 \
      --campos "$pos" --camtgt "$tgt" --hfov "$fov" --shift 0 --out "$OUT/$name" "$@"
  fi
  python3 tools/post.py "$OUT/$name" "$DOCS/$name.jpg" --ev "$ev" --quality 90
}
d gate_lantern_0730  street 07:30      1.0 13.9,2.7,0.9   12.2,2.55,-0.9  36
d street_end_0730    street 07:30      1.0 15.8,1.62,2.0  13.4,1.9,-30.0  30
d lantern_noon       court  12:20:42.5 0.4 1.9,2.2,6.9    3.6,0.0,8.8     60 --level 0
d pool_noon          court  12:20:42.5 0.4 2.3,1.55,4.6   3.6,0.0,6.6     55 --level 0
d olives_noon        court  12:20:42.5 0.4 1.2,1.6,9.2    3.2,1.2,12.6    55 --level 0
d door_1700          court  17:00      1.3 5.0,1.5,6.6    7.2,1.35,9.4    58
d bench_0800         court  08:00      1.2 2.3,1.35,3.4   0.2,0.45,5.6    58 --level 0
echo "details done"
