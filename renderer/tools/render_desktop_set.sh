#!/usr/bin/env bash
# The desktop set: every landscape frame of both sequences (1600x900 and
# 1280x720 tiers plus the ladder, and the walk's depth maps). Started only
# after the scrub test (check 2) has passed on the phone set. Resumable:
# finished frames are skipped; every 6 frames are committed and pushed.
#
# Even frames first: they make a complete desktop set on their own (the
# player blends across the missing odd frames). The odd frames double the
# density and cost about 6 hours on this machine, which would take the whole
# render past the 30 hours agreed, so they run only when asked for
# (ZAWAL_ODD=1).
set -uo pipefail
cd "$(dirname "$0")/.."
export ZAWAL_SCRATCH=${ZAWAL_SCRATCH:-/tmp/zawal-seq}
make -s
python3 tools/render_seq.py --seq b --variant landscape --frames even --commit
python3 tools/render_seq.py --seq a --variant landscape --frames even --commit
echo "desktop even set done"
if [ -n "${ZAWAL_ODD:-}" ]; then
  python3 tools/render_seq.py --seq b --variant landscape --frames odd --commit
  python3 tools/render_seq.py --seq a --variant landscape --frames odd --commit
  echo "desktop set done"
fi
