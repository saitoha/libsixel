#!/bin/sh
# TAP test ensuring temporal-diffusion is rejected in high-color mode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

set +e
error_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -I -d temporal-diffusion "${input_image}" 2>&1 >/dev/null
)
status=$?
set -e

test "${status}" -ne 0 || {
    echo "not ok" 1 - "high-color mode unexpectedly accepted temporal-diffusion"
    exit 0
}

test "${error_output#*temporal-diffusion*}" != "${error_output}" || {
    echo "not ok" 1 - "error message for temporal-diffusion rejection is missing"
    exit 0
}

echo "ok" 1 - "high-color mode rejects temporal-diffusion"
exit 0
