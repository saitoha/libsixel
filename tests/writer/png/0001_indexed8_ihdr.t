#!/bin/sh
# Verify the default decoder path writes an indexed 8-bit PNG.
# Policy: docs/writers/png.md

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

png_path="${ARTIFACT_LOCAL_DIR}/indexed8.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
        <"${TOP_SRCDIR}/images/map8.six" >"${png_path}" || {
    echo "not ok" 1 - "indexed PNG conversion failed"
    exit 0
}

expected_ihdr_format_cksum="2320760698 2"
actual_ihdr_format_cksum=$(dd bs=1 skip=24 count=2 if="${png_path}" \
        2>/dev/null | cksum)

test "${actual_ihdr_format_cksum}" = "${expected_ihdr_format_cksum}" || {
    echo "not ok" 1 - "default output is not indexed 8-bit PNG"
    exit 0
}

echo "ok" 1 - "default output is indexed 8-bit PNG"
exit 0
