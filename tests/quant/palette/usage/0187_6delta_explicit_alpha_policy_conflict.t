#!/bin/sh
# Verify 6delta rejects an explicitly incompatible alpha policy.
# Policy: docs/functionality/delta-encoding.md
# Policy: docs/loader/alpha-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +e

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=clear --6delta-threshold=0 \
    -o /dev/null "${input_image}" 2>&1)
status=$?
set -e

test "${status}" -ne 0 || {
    echo "not ok" 1 - "6delta accepted explicit alpha-policy=clear"
    exit 0
}

case "${output}" in
    *"6delta requires alpha-policy=auto or keep"*) ;;
    *)
        echo "not ok" 1 - "6delta alpha-policy conflict message mismatch"
        exit 0
        ;;
esac

echo "ok" 1 - "6delta rejects explicit incompatible alpha policy"
exit 0
