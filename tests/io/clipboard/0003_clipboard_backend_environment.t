#!/bin/sh
# Pin the legacy file/fake clipboard backend environment spellings.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
file_dir="${ARTIFACT_LOCAL_DIR}/clipboard-file-$$"
fake_dir="${ARTIFACT_LOCAL_DIR}/clipboard-fake-$$"
mkdir -p "${file_dir}" "${fake_dir}"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_CLIPBOARD_BACKEND=FiLe \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${file_dir}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o png:clipboard: || {
    echo "not ok" 1 - "mixed-case file backend was not selected"
    exit 0
}
test -s "${file_dir}/image.bin" || {
    echo "not ok" 1 - "file backend did not write its image slot"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_CLIPBOARD_BACKEND=FaKe \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${fake_dir}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o png:clipboard: || {
    echo "not ok" 1 - "legacy fake backend was not selected"
    exit 0
}
test -s "${fake_dir}/image.bin" || {
    echo "not ok" 1 - "legacy fake backend did not write its image slot"
    exit 0
}

echo "ok" 1 - "clipboard backend accepts file and legacy fake case-insensitively"
exit 0
