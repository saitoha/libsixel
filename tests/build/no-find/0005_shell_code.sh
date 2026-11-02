#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
src_root=$1
status=0
awk -f "$src_root/tests/_static/awk/shell-words.awk" \
    -f "$src_root/tests/_static/awk/no-find.awk" <<'SPECIMEN' || status=$?
sh -c 'find .'
SPECIMEN
test "$status" -ne 0
