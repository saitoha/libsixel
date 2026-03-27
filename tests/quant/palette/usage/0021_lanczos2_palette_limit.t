#!/bin/sh
# Scale with Lanczos2 filter while limiting palette size.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos2-palette-limit.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -C3 -h100 -fnorm -rlanczos2 "${snake_jpg}" \
        >"${target_sixel}" || {
    echo "not ok" 1 - "Lanczos2 scaling with palette limit fails"
    exit 0
}

echo "ok" 1 - "Lanczos2 scaling with palette limit succeeds"

exit 0
