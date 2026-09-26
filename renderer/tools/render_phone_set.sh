#!/usr/bin/env bash
# The phone set: every portrait frame of both sequences (phones scrub all
# 121 frames of the day and all 97 of the walk: at every 2nd frame the
# canopy's stars jump ~55 % of their period, past the point where a blend
# still reads as motion). Resumable: finished frames are skipped, every 6
# frames are committed and pushed.
#
# Order: A first (the scrub test needs it earliest), then B. The desktop set
# (render_desktop_set.sh) runs only after the scrub has been verified here.
set -uo pipefail
cd "$(dirname "$0")/.."
export ZAWAL_SCRATCH=${ZAWAL_SCRATCH:-/tmp/zawal-seq}
make -s
python3 tools/render_seq.py --seq b --variant portrait --only 0 --commit --chunk 1
python3 tools/render_seq.py --seq a --variant portrait --frames all --commit
python3 tools/render_seq.py --seq b --variant portrait --frames all --commit
echo "phone set done"
