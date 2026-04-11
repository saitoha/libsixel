#!/bin/sh
# TAP test verifying non-stbn diffusion rejects scene_cut_reset key.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -d fs:scene_cut_reset=1 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    echo "not ok" 1 - "non-stbn scene_cut_reset unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown suboption key*}" != "${msg}" || {
    echo "not ok" 1 - "missing non-stbn scene_cut_reset diagnostic"
    exit 0
}

test "${msg#*scene_cut_reset*}" != "${msg}" || {
    echo "not ok" 1 - "non-stbn scene_cut_reset diagnostic lacked key name"
    exit 0
}

echo "ok" 1 - "non-stbn scene_cut_reset suboption is rejected"
exit 0
