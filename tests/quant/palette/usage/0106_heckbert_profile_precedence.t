#!/bin/sh
# Ensure explicit heckbert options override profile defaults.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..2"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
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
run_status=0

set +x
test "${baseline_none_norm}" = "${profile_none_norm}" || {
    set -x
    echo "not ok" 1 - "profile=quality overrode explicit merge=none/-f"
    exit 0
}
set -x
echo "ok" 1 - "explicit merge=none and -f override profile=quality"

set +x
baseline_ward_pca=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Q "heckbert:merge=ward" \
        -f pca \
        -p 16 \
        "${input_image}" | cksum
) || run_status=$?
set -x

test "${run_status}" -eq 0 || {
    echo "not ok" 2 - "baseline merge=ward encode failed"
    exit 0
}
run_status=0

set +x
profile_ward_pca=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Q "heckbert:profile=speed:merge=ward" \
        -f pca \
        -p 16 \
        "${input_image}" | cksum
) || run_status=$?
set -x

test "${run_status}" -eq 0 || {
    echo "not ok" 2 - "profile merge=ward encode failed"
    exit 0
}
run_status=0

set +x
test "${baseline_ward_pca}" = "${profile_ward_pca}" || {
    set -x
    echo "not ok" 2 - "profile=speed overrode explicit merge=ward/-f"
    exit 0
}
set -x
echo "ok" 2 - "explicit merge=ward and -f override profile=speed"

exit 0
