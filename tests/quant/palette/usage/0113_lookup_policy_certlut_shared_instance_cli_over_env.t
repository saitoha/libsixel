#!/bin/sh
# Verify certlut shared_instance suboption works with an env override present.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
baseline_cksum=''
override_cksum=''
run_status=0

set +x
baseline_cksum=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE=0 \
        --lookup-policy=certlut \
        -p 16 \
        "${input_image}" | cksum
) || run_status=$?
set -x

test "${run_status}" -eq 0 || {
    echo "not ok" 1 - "baseline certlut command failed"
    exit 0
}
run_status=0

set +x
override_cksum=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE=1 \
        --lookup-policy=certlut:shared_instance=0 \
        -p 16 \
        "${input_image}" | cksum
) || run_status=$?
set -x

test "${run_status}" -eq 0 || {
    echo "not ok" 1 - "certlut CLI shared_instance override command failed"
    exit 0
}

test "${baseline_cksum}" = "${override_cksum}" || {
    echo "not ok" 1 - "certlut CLI shared_instance override changed output"
    exit 0
}

echo "ok" 1 - "certlut shared_instance CLI suboption works with env"

exit 0
