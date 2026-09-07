#!/bin/sh
# Verify alpha-policy selection is stable across multi-input sequences.

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
out_keep="${ARTIFACT_LOCAL_DIR}/builtin-alpha-policy-multi-keep.six"
out_composite="${ARTIFACT_LOCAL_DIR}/builtin-alpha-policy-multi-composite.six"
out_repeated="${ARTIFACT_LOCAL_DIR}/builtin-alpha-policy-multi-repeated.six"

# Keep the policy-sensitive PNG first in the sequence.
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_ALPHA_POLICY=keep \
              --env SIXEL_THREADS=4 -Lbuiltin! -d fs:scan=raster -B#fff -p 2 \
              "${input_b}" "${input_a}" >"${out_keep}" || {
    echo "not ok 1 - builtin multi-input alpha-policy=keep render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_ALPHA_POLICY=composite \
              --env SIXEL_THREADS=4 -Lbuiltin! -d fs:scan=raster -B#fff -p 2 \
              "${input_b}" "${input_a}" >"${out_composite}" || {
    echo "not ok 1 - builtin multi-input alpha-policy=composite render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
              --env SIXEL_ALPHA_POLICY=keep \
              --env SIXEL_ALPHA_POLICY=composite \
              --env SIXEL_THREADS=4 -Lbuiltin! -d fs:scan=raster -B#fff -p 2 \
              "${input_b}" "${input_a}" >"${out_repeated}" || {
    echo "not ok 1 - builtin multi-input repeated alpha-policy render failed"
    exit 0
}

cmp -s "${out_keep}" "${out_composite}" && {
    echo "not ok 1 - builtin multi-input alpha-policy switch had no effect"
    exit 0
}

cmp -s "${out_repeated}" "${out_composite}" || {
    echo "not ok 1 - builtin multi-input repeated alpha-policy did not use last value"
    exit 0
}

echo "ok 1 - builtin multi-input alpha-policy sequencing is stable"

exit 0
