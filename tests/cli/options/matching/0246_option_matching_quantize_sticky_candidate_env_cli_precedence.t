#!/bin/sh
# TAP test verifying sticky candidate accepts env/CLI values with CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_STICKY_CANDIDATE=medoids" \
    -Qsticky \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only sticky candidate=medoids was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qsticky:candidate=heckbert \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only sticky candidate=heckbert was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_STICKY_CANDIDATE=medoids" \
    -Qsticky:candidate=invalid \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI sticky candidate unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption value*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid sticky candidate diagnostic"
    exit 0
}

test "${msg#*\"candidate\"*}" != "${msg}" || {
    echo "not ok" 1 - "missing sticky candidate key diagnostic"
    exit 0
}

test "${msg#*valid values*medoids, heckbert*}" != "${msg}" || {
    echo "not ok" 1 - "missing sticky candidate value list"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_STICKY_CANDIDATE=invalid" \
    -Qsticky:candidate=medoids \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI sticky candidate did not override env"
    exit 0
}

echo "ok" 1 - "sticky candidate follows env/CLI acceptance and CLI priority"
exit 0
