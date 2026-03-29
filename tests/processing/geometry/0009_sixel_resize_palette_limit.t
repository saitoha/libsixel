#!/bin/sh
# Resize Sixel input while constraining palette size.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_six="${TOP_SRCDIR}/images/map8.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-resize.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -w200 -p8 "${snake_six}" \
        >"${target_sixel}" || {
    echo "not ok" 1 - "Sixel resizing with palette limit fails"
    exit 0
}

echo "ok" 1 - "Sixel resizing with palette limit succeeds"

exit 0
