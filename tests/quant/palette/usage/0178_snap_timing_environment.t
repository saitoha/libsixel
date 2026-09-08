#!/bin/sh
# Verify the snap timing policy reaches the palette snap consumer.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/0178-snap-timing-$$.six"

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_PALETTE_SNAP_TIMING_POLICY=all \
    -p 16 -Qkmeans -_nearest -W oklab \
    "${input_image}" 2>&1 >"${output_sixel}") || {
    echo "not ok 1 - snap timing conversion failed"
    exit 0
}

test "${trace#*LSXSNP1|timing=all*}" != "${trace}" || {
    echo "not ok 1 - snap timing missed its consumer"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${output_sixel}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok 1 - snap timing image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok 1 - snap timing preserves image output"
exit 0
