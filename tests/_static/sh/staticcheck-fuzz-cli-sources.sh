#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Policy: docs/ci/design.md
set -eux
echo '1..1'
set -v
src_root=$1
checker=$src_root/tests/_static/awk/fuzz-cli-sources.awk
awk -f "$checker" "$src_root/converters/Makefile.am" "$src_root/Makefile.am" >&2 || {
    echo 'not ok 1 - embedded CLI fuzz source registration'
    exit 0
}
# Mutate the real inventory: another valid filename must not hide an omission.
{ cat "$src_root/converters/Makefile.am"; sed 's@terminal-color-job.c@missing.c@g' "$src_root/Makefile.am"; } |
    awk -f "$checker" >&2 && {
    echo 'not ok 1 - missing fuzz source escaped detection'
    exit 0
}
echo 'ok 1 - embedded CLI fuzz source registration'
exit 0
