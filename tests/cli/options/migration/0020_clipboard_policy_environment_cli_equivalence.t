#!/bin/sh
# Verify clipboard backend selection is equivalent through CLI and env paths.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
cli_dir="${ARTIFACT_LOCAL_DIR}/clipboard-policy-cli-$$"
env_dir="${ARTIFACT_LOCAL_DIR}/clipboard-policy-env-$$"
mkdir -p "${cli_dir}" "${env_dir}"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_CLIPBOARD_BACKEND=system \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${cli_dir}" \
    -yfile -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o png:clipboard: || {
    echo "not ok" 1 - "clipboard policy CLI conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_CLIPBOARD_BACKEND=FaKe \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${env_dir}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o png:clipboard: || {
    echo "not ok" 1 - "clipboard policy environment conversion failed"
    exit 0
}

cmp -s "${cli_dir}/image.bin" "${env_dir}/image.bin" || {
    echo "not ok" 1 - "clipboard policy CLI and environment outputs differ"
    exit 0
}

echo "ok" 1 - "clipboard policy CLI overrides and matches its environment"
exit 0
