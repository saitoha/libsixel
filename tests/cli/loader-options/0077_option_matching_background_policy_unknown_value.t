#!/bin/sh
# Verify -N/--background-policy rejects an unknown value.
# Policy: docs/loader/background-policy.md

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
    -N unknown -o /dev/null \
    "${input_image}" 2>&1)
status=$?
set -e

test "${status}" -ne 0 || {
    printf '\n%s\n' "not ok 1 - background-policy accepted an unknown value"
    exit 0
}

case "${output}" in
    *"valid values: file_first, explicit_first"*) ;;
    *)
        printf '\n%s\n' "not ok 1 - background-policy valid-value diagnostic mismatch"
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - background-policy rejects an unknown value"
exit 0
