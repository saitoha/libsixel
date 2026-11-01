#!/bin/sh
# Crop Sixel input with large offsets tolerated.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_six="${TOP_SRCDIR}/images/map8.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-crop-offsets.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -c200x200+2000+2000 "${snake_six}" >"${target_sixel}" || {
    echo "not ok" 1 - "Sixel cropping with large offsets fails"
    exit 0
}

echo "ok" 1 - "Sixel cropping tolerates large offsets"

exit 0
