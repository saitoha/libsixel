#!/bin/sh
# Verify unpublished scalar delta options are not compatibility aliases.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --6delta-threshold=0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" >/dev/null 2>&1 || status=$?
test "$status" -ne 0 || {
    echo "not ok 1 - removed delta option accepted: --6delta-threshold=0"
    exit 0
}

status=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --6delta-error=skip \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" >/dev/null 2>&1 || status=$?
test "$status" -ne 0 || {
    echo "not ok 1 - removed delta option accepted: --6delta-error=skip"
    exit 0
}

status=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Yskip \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" >/dev/null 2>&1 || status=$?
test "$status" -ne 0 || {
    echo "not ok 1 - removed delta option accepted: -Yskip"
    exit 0
}

status=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Z0 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" >/dev/null 2>&1 || status=$?
test "$status" -ne 0 || {
    echo "not ok 1 - removed delta option accepted: -Z0"
    exit 0
}


echo "ok 1 - removed delta options are rejected"
exit 0
