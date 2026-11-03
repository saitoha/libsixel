#!/bin/sh
# Do not create a shell record file for converter output nobody inspects.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.t' --exclude-path '*/_artifacts/*' --exec awk \
    -f "$src_root/tests/_static/awk/cli-discard-output.awk" >&2 || {
    echo 'not ok 1 - converter stdout captures have consumers'
    exit 0
}
echo 'ok 1 - converter stdout captures have consumers'
exit 0
