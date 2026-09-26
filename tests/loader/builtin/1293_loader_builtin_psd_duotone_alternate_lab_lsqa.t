#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Policy: docs/misc/platforms/openvms.md
# Compare the Duotone appearance table with an independent Lab-derived PPM.
# Regeneration: tests/data/psd-tools/generate_duotone_reference.py INPUT OUTPUT

# Coverage: OV-13
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - Duotone Lab-table quality # SKIP img2sixel unavailable"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# A regular file avoids GNV retaining the writer side of a short pipe.
output_sixel="${ARTIFACT_LOCAL_DIR}/1293-duotone.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -Ugamma \
    -o "${output_sixel}" \
    "${TOP_SRCDIR}/tests/data/psd-tools/psdtools_colormodes_4x4_8bit_duotone.psd" || {
    echo "not ok 1 - Duotone Lab-table decode failed"
    exit 0
}
${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    --cms-engine none -m MS-SSIM -W linear \
    -b MS-SSIM:0.995 \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psdtools_colormodes_4x4_8bit_duotone_expected_lab.ppm" \
    "${output_sixel}" >/dev/null || {
    echo "not ok 1 - Duotone Lab-table quality below 0.995"
    exit 0
}
echo "ok 1 - Duotone Lab-table quality meets 0.995"
exit 0
