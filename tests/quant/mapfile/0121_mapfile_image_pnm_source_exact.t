#!/bin/sh
# Verify a PNM image can construct a deterministic mapfile palette.
# Test-plan: docs/testing/mapfile-parser-coverage.md
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
mapfile_image="${ARTIFACT_LOCAL_DIR}/map.ppm"
expected_palette='JASC-PAL
0100
2
2 2 2
255 255 255'

printf 'P3\n2 1\n255\n0 0 0 255 255 255\n' >"${mapfile_image}"

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
        --gpu-policy=off -Wgamma -Ugamma -m "${mapfile_image}" \
        -M pal-jasc:- -o/dev/null "${input_image}"
) || {
    echo "not ok" 1 - "PNM image mapfile conversion failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "PNM mapfile output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "PNM image constructed an unexpected map palette"
    exit 0
}

echo "ok" 1 - "PNM image constructs a deterministic mapfile palette"
exit 0
