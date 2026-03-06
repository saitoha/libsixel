#!/bin/sh
# Confirm img2sixel -%/--env behaves like process environment variables.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
out_env="${ARTIFACT_LOCAL_DIR}/img2sixel_env_ref.six"
out_opt="${ARTIFACT_LOCAL_DIR}/img2sixel_env_opt.six"

run_img2sixel --env SIXEL_COLORS=16 --env SIXEL_THREADS=1 \
    "${input_image}" >"${out_env}" || {
    echo "not ok" 1 - "reference environment conversion failed"
    exit 0
}

run_img2sixel -% SIXEL_COLORS=16 -% SIXEL_THREADS=1 \
    "${input_image}" >"${out_opt}" || {
    echo "not ok" 1 - "-% conversion failed"
    exit 0
}

cmp -s "${out_env}" "${out_opt}" || {
    echo "not ok" 1 - "-% output differs from process environment"
    exit 0
}

run_img2sixel -% INVALID "${input_image}" > /dev/null && {
    echo "not ok" 1 - "invalid -% argument should fail"
    exit 0
}

echo "ok" 1 - "invalid -% argument rejected"
exit 0
