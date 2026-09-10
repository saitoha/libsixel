#!/bin/sh
# Policy: docs/functionality/decoding-pipeline.md
# Verify dequantized decoder output is written as 8-bit RGB PNG.
# Policy: docs/writers/png.md

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

png_path="${ARTIFACT_LOCAL_DIR}/rgb8.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dk_undither \
        <"${TOP_SRCDIR}/images/map8.six" >"${png_path}" || {
    echo "not ok" 1 - "RGB PNG conversion failed"
    exit 0
}

expected_ihdr_format_cksum="1481260710 2"
actual_ihdr_format_cksum=$(dd bs=1 skip=24 count=2 if="${png_path}" \
        2>/dev/null | cksum)

test "${actual_ihdr_format_cksum}" = "${expected_ihdr_format_cksum}" || {
    echo "not ok" 1 - "dequantized output is not 8-bit RGB PNG"
    exit 0
}

echo "ok" 1 - "dequantized output is 8-bit RGB PNG"
exit 0
