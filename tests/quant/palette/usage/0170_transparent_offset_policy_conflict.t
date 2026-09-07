#!/bin/sh
# Verify transparent-offset rejects composite alpha handling.
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
    -L builtin -p 2 --alpha-policy=composite -+ 3,5 \
    -o - "${input_image}" 2>&1 >/dev/null)
status=$?
set -e

test "${status}" -ne 0 || {
    echo "not ok 1 - transparent-offset accepted composite policy"
    exit 0
}

case "${output}" in
    *"transparent-offset requires alpha-policy=auto or keep"*) ;;
    *)
        echo "not ok 1 - transparent-offset conflict message mismatch"
        exit 0
        ;;
esac

echo "ok 1 - transparent-offset rejects composite policy"
exit 0
