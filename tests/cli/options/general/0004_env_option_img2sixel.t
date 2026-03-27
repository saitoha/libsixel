#!/bin/sh
# Confirm img2sixel -%/--env behaves like process environment variables.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
out_env="${ARTIFACT_LOCAL_DIR}/img2sixel_env_ref.six"
out_opt="${ARTIFACT_LOCAL_DIR}/img2sixel_env_opt.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_COLORS=16 --env SIXEL_THREADS=1 \
    "${input_image}" >"${out_env}" || {
    echo "not ok" 1 - "reference environment conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -% SIXEL_COLORS=16 -% SIXEL_THREADS=1 \
    "${input_image}" >"${out_opt}" || {
    echo "not ok" 1 - "-% conversion failed"
    exit 0
}

cmp -s "${out_env}" "${out_opt}" || {
    echo "not ok" 1 - "-% output differs from process environment"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -% INVALID "${input_image}" > /dev/null && {
    echo "not ok" 1 - "invalid -% argument should fail"
    exit 0
}

echo "ok" 1 - "invalid -% argument rejected"
exit 0
