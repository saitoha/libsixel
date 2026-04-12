#!/bin/sh
# Verify transparent policy selection is stable across multi-input sequences.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_a="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_dispose_background.png"
input_b="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out_transparent="${ARTIFACT_LOCAL_DIR}/builtin-transparent-policy-multi-transparent.six"
out_composite="${ARTIFACT_LOCAL_DIR}/builtin-transparent-policy-multi-composite.six"
out_repeated="${ARTIFACT_LOCAL_DIR}/builtin-transparent-policy-multi-repeated.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRANSPARENT_POLICY=transparent \
              --env SIXEL_THREADS=4 -Lbuiltin! -d fs:scan=raster -B#fff \
              "${input_a}" "${input_b}" >"${out_transparent}" || {
    echo "not ok 1 - builtin multi-input transparent-policy=transparent render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRANSPARENT_POLICY=composite \
              --env SIXEL_THREADS=4 -Lbuiltin! -d fs:scan=raster -B#fff \
              "${input_a}" "${input_b}" >"${out_composite}" || {
    echo "not ok 1 - builtin multi-input transparent-policy=composite render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
              --env SIXEL_TRANSPARENT_POLICY=transparent \
              --env SIXEL_TRANSPARENT_POLICY=composite \
              --env SIXEL_THREADS=4 -Lbuiltin! -d fs:scan=raster -B#fff \
              "${input_a}" "${input_b}" >"${out_repeated}" || {
    echo "not ok 1 - builtin multi-input repeated transparent-policy render failed"
    exit 0
}

cmp -s "${out_transparent}" "${out_composite}" && {
    echo "not ok 1 - builtin multi-input transparent-policy switch had no effect"
    exit 0
}

cmp -s "${out_repeated}" "${out_composite}" || {
    echo "not ok 1 - builtin multi-input repeated transparent-policy did not use last value"
    exit 0
}

echo "ok 1 - builtin multi-input transparent-policy sequencing is stable"

exit 0
