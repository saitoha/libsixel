#!/bin/sh
# Verify 6delta rejects composite alpha handling.
# Policy: docs/encoder/output/delta-encoding.md
# Policy: docs/loader/alpha-policy/README.md

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
    --alpha-policy=composite --update-policy=delta:threshold=0 \
    -o /dev/null "${input_image}" 2>&1)
status=$?
set -e

test "${status}" -ne 0 || {
    printf '\n%s\n' "not ok 1 - 6delta accepted alpha-policy=composite"
    exit 0
}

case "${output}" in
    *"6delta requires alpha-policy=auto or keep"*) ;;
    *)
        printf '\n%s\n' "not ok 1 - 6delta alpha-policy conflict message mismatch"
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - 6delta rejects composite policy"
exit 0
