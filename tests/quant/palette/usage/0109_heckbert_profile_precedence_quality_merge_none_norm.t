#!/bin/sh
# Ensure explicit merge=none and -f norm override profile=quality defaults.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
baseline_none_norm=''
profile_none_norm=''
run_status=0

set +x
baseline_none_norm=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Q "heckbert:merge=none" \
        -f norm \
        -p 16 \
        "${input_image}" | cksum
) || run_status=$?
set -x

test "${run_status}" -eq 0 || {
    echo "not ok" 1 - "baseline merge=none encode failed"
    exit 0
}
run_status=0

set +x
profile_none_norm=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Q "heckbert:profile=quality:merge=none" \
        -f norm \
        -p 16 \
        "${input_image}" | cksum
) || run_status=$?
set -x

test "${run_status}" -eq 0 || {
    echo "not ok" 1 - "profile merge=none encode failed"
    exit 0
}
test "${baseline_none_norm}" = "${profile_none_norm}" || {
    echo "not ok" 1 - "profile=quality overrode explicit merge=none/-f"
    exit 0
}
echo "ok" 1 - "explicit merge=none and -f override profile=quality"

exit 0
