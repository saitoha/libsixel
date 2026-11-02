#!/bin/sh
# Pattern predicates use case; exceptions require explicit user approval.
# Policy: docs/testing/guide.md
set -eux
echo '1..1'
set -v
src_root=$1
LC_ALL=C
export LC_ALL
inventory=$(mktemp "${TMPDIR:-/tmp}/libsixel-pattern-files-XXXXXX")
trap 'rm -f "$inventory"' EXIT HUP INT TERM
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.t' --exclude-path '*/data/*' > "$inventory" || {
    echo 'not ok 1 - cannot enumerate shell TAP sources'
    exit 0
}
awk -v root="$src_root" \
    -f "$src_root/tests/_static/awk/shell-words.awk" \
    -f "$src_root/tests/_static/awk/sha256.awk" \
    -f "$src_root/tests/_static/awk/test-pattern-whitelist.awk" \
    -f "$src_root/tests/_static/awk/test-pattern-controls.awk" \
    -f "$src_root/tests/_static/awk/test-pattern-match.awk" "$inventory" || {
    echo 'not ok 1 - shell pattern predicates use case or approved exceptions'
    exit 0
}
echo 'ok 1 - shell pattern predicates use case or approved exceptions'
exit 0
