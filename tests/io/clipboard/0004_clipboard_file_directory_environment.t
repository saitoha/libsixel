#!/bin/sh
# Pin the file clipboard directory and its empty-value behavior.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
clipboard_dir="${ARTIFACT_LOCAL_DIR}/clipboard-directory-$$"
mkdir -p "${clipboard_dir}"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_CLIPBOARD_BACKEND=file \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${clipboard_dir}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o png:clipboard: || {
    echo "not ok" 1 - "file clipboard directory was not used"
    exit 0
}
test -s "${clipboard_dir}/image.bin" || {
    echo "not ok" 1 - "file clipboard directory lacks its image slot"
    exit 0
}

! ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_CLIPBOARD_BACKEND=file \
    --env SIXEL_CLIPBOARD_FILE_DIR= \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o png:clipboard: >/dev/null 2>&1 || {
    echo "not ok" 1 - "empty file clipboard directory was accepted"
    exit 0
}

echo "ok" 1 - "file clipboard directory preserves populated and empty behavior"
exit 0
