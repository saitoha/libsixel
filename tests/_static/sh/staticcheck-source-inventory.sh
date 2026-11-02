#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Source enumeration must be complete and independent of Python availability.
set -eux
echo '1..1'
set -v
src_root=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-inventory-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/tree with spaces/deep" "$work/tree with spaces/.hidden" \
    "$work/tree with spaces/_artifacts" "$work/bin"
root=$work/tree\ with\ spaces
: > "$root/one.c"
: > "$root/ignore.h"
: > "$root/deep/two.c"
: > "$root/deep/three.txt"
: > "$root/.hidden/hidden.c"
: > "$root/_artifacts/generated.c"
ln -s "$root/one.c" "$root/link.c"
printf '%s\n' '#!/bin/sh' 'echo "unexpected Python dependency" >&2' 'exit 125' > "$work/bin/python3"
chmod +x "$work/bin/python3"
ln -s python3 "$work/bin/python"
PATH=$work/bin:$PATH
PYTHON_STATICCHECK=$work/bin/python3
export PATH PYTHON_STATICCHECK
scanner=$src_root/tests/_static/sh/list-source-files.sh
sh "$scanner" "$root" --name '*.c' > "$work/direct" || {
    echo 'not ok 1 - source inventory requires Python'
    exit 0
}
printf '%s\n' "$root/deep/two.c" "$root/one.c" > "$work/expected"
LC_ALL=C sort "$work/direct" > "$work/actual"
cmp "$work/expected" "$work/actual" || {
    echo 'not ok 1 - source inventory changed membership'
    exit 0
}
sh "$scanner" "$root" > "$work/snapshot"
SIXEL_STATICCHECK_INVENTORY_ROOT=$root
SIXEL_STATICCHECK_TEST_INVENTORY=$work/snapshot
export SIXEL_STATICCHECK_INVENTORY_ROOT SIXEL_STATICCHECK_TEST_INVENTORY
sh "$scanner" "$root" --name '*.c' > "$work/cached"
cmp "$work/direct" "$work/cached" || {
    echo 'not ok 1 - cached inventory changed source paths'
    exit 0
}
sh "$scanner" "$root" --name '*.c' --max-depth 1 --null > "$work/actual"
printf '%s\0' "$root/one.c" > "$work/expected"
cmp "$work/expected" "$work/actual" || {
    echo 'not ok 1 - cached inventory lost depth or NUL boundaries'
    exit 0
}
sh "$scanner" "$root" --name '*.[ch]' --exclude-name 'ignore.*' \
    --exclude-path "$root/deep" > "$work/actual"
printf '%s\n' "$root/one.c" > "$work/expected"
cmp "$work/expected" "$work/actual" || {
    echo 'not ok 1 - cached inventory lost glob exclusions'
    exit 0
}
sh "$scanner" "$root" --name '*.c' --max-depth 0 > "$work/actual"
printf '%s\n' "$root/one.c" > "$work/expected"
cmp "$work/expected" "$work/actual" || {
    echo 'not ok 1 - explicit zero depth unexpectedly traversed children'
    exit 0
}
# An error must not invoke the consumer with a plausible partial inventory.
status=0
sh "$scanner" "$root" "$work/missing" --exec sh -c 'exit 0' > /dev/null 2>&1 || status=$?
test "$status" -ne 0 || {
    echo 'not ok 1 - missing source root was silently accepted'
    exit 0
}
sh "$scanner" "$root" --name '*.c' --exec printf '%s\n' > "$work/actual"
cmp "$work/direct" "$work/actual" || {
    echo 'not ok 1 - inventory execution lost argument boundaries'
    exit 0
}
# Metadata checks explicitly include links; cached regular-file lists must
# not hide a symlink masquerading as a test source.
sh "$scanner" "$root" --name 'link.c' --include-links > "$work/actual"
printf '%s\n' "$root/link.c" > "$work/expected"
cmp "$work/expected" "$work/actual" || {
    echo 'not ok 1 - metadata inventory lost a symbolic link'
    exit 0
}
echo 'ok 1 - source enumeration preserves membership without Python'
exit 0
