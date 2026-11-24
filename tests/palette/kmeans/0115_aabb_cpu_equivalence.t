#!/bin/sh
# Check that AABB preserves exhaustive CPU output.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo '1..1'
set -v
set +xv
plain=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -Fnone -p 64 \
    -Qkmeans:backend=cpu:prune=none:seed=1:iter=5 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo 'not ok 1 - exhaustive CPU conversion'
    exit 0
}
aabb=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -Fnone -p 64 \
    -Qkmeans:backend=cpu:prune=aabb:seed=1:iter=5 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo 'not ok 1 - AABB CPU conversion'
    exit 0
}
test -n "$plain" && test "$plain" = "$aabb" || {
    echo 'not ok 1 - AABB CPU output differs from exhaustive CPU output'
    exit 0
}
echo 'ok 1 - AABB CPU output matches exhaustive CPU output'
exit 0
