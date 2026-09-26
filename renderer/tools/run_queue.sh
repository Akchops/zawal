#!/bin/bash
# Runs the lines of a queue file one after another, in order. Lines may be
# appended while it runs; a line reading END stops it. Finished lines are
# listed in <queue>.done, so a restart resumes after the last finished one.
Q=$1; DONE=$Q.done; touch "$DONE"
while true; do
  n=$(wc -l < "$DONE")
  line=$(sed -n "$((n + 1))p" "$Q")
  if [ -z "$line" ]; then sleep 20; continue; fi
  [ "$line" = "END" ] && { echo "== queue finished $(date +%T)"; exit 0; }
  echo "== $(date +%T) start: $line"
  bash -c "$line"; rc=$?
  echo "== $(date +%T) rc=$rc: $line"
  echo "$line" >> "$DONE"
done
