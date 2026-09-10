#!/bin/sh
# Verify the builtin writer uses the PNG reflected CRC-32 convention.
# Policy: docs/writers/png.md

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-0}" != 1 || {
    printf "1..0 # SKIP builtin PNG writer is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

png_path="${ARTIFACT_LOCAL_DIR}/builtin-crc.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
        <"${TOP_SRCDIR}/images/map8.six" >"${png_path}" || {
    echo "not ok" 1 - "builtin PNG conversion failed"
    exit 0
}

expected_ihdr_crc_cksum="737776243 4"
actual_ihdr_crc_cksum=$(dd bs=1 skip=29 count=4 if="${png_path}" \
        2>/dev/null | cksum)

test "${actual_ihdr_crc_cksum}" = "${expected_ihdr_crc_cksum}" || {
    echo "not ok" 1 - "builtin IHDR CRC does not match PNG CRC-32"
    exit 0
}

echo "ok" 1 - "builtin IHDR CRC matches PNG CRC-32"
exit 0
