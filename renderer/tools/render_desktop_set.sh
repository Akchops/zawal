#!/usr/bin/env bash
# The desktop set: every landscape frame of both sequences (1600x900 and
# 1280x720 tiers plus the ladder, and the walk's depth maps). Started only
# after the scrub test (check 2) has passed on the phone set. Resumable:
# finished frames are skipped; every 6 frames are committed and pushed.
set -uo pipefail
cd "$(dirname "$0")/.."
export ZAWAL_SCRATCH=${ZAWAL_SCRATCH:-/tmp/zawal-seq}
make -s
python3 tools/render_seq.py --seq b --variant landscape --frames all --commit
python3 tools/render_seq.py --seq a --variant landscape --frames all --commit
echo "desktop set done"
