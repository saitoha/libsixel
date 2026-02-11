#!/bin/sh
# TAP test ensuring img2sixel reports suggestions for missing input paths.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

missing_input="${images_dir}/snake.jpp"
stderr_capture="${ARTIFACT_LOCAL_DIR}/err.txt"

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
    -i "${missing_input}" \
    >"${ARTIFACT_LOCAL_DIR}/output.txt" \
    2>"${stderr_capture}" && {
    fail 1 "accepts missing input path"
    exit 0
}

grep -q "Suggestions:" "${stderr_capture}" || {
    fail 1 "missing input path did not include suggestion header"
    exit 0
}

grep -q "modified" "${stderr_capture}" || {
    fail 1 "missing input path did not include modified timestamp hint"
    exit 0
}

pass 1 "missing input path includes suggestion diagnostics"
exit 0
