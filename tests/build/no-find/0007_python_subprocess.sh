#!/bin/sh
# Policy: docs/testing/guide.md
# Literal Python process arguments must not hide findutils dependencies.
set -eux
src_root=$1
status=0
awk -v input_language=python \
    -f "$src_root/tests/_static/awk/shell-words.awk" \
    -f "$src_root/tests/_static/awk/no-find.awk" <<'SPECIMEN' || status=$?
subprocess.run(['find', '.', '-type', 'f'])
SPECIMEN
test "$status" -ne 0
awk -v input_language=python \
    -f "$src_root/tests/_static/awk/shell-words.awk" \
    -f "$src_root/tests/_static/awk/no-find.awk" <<'SPECIMEN'
# subprocess.run(['find', '.'])
message = "subprocess.run(['find', '.'])"
subprocess.run(['printf', 'find'])
SPECIMEN
