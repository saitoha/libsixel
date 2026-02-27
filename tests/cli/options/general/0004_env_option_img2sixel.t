#!/bin/sh
# Confirm img2sixel -%/--env behaves like process environment variables.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..2"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
out_env="${ARTIFACT_LOCAL_DIR}/img2sixel_env_ref.six"
out_opt="${ARTIFACT_LOCAL_DIR}/img2sixel_env_opt.six"
err_invalid="${ARTIFACT_LOCAL_DIR}/img2sixel_env_invalid.err"

run_img2sixel --env SIXEL_COLORS=16 --env SIXEL_THREADS=1 \
    "${input_image}" >"${out_env}" || {
    fail 1 "reference environment conversion failed"
    exit 0
}

run_img2sixel -% SIXEL_COLORS=16 -% SIXEL_THREADS=1 \
    "${input_image}" >"${out_opt}" || {
    fail 1 "-% conversion failed"
    exit 0
}

cmp -s "${out_env}" "${out_opt}" || {
    fail 1 "-% output differs from process environment"
    exit 0
}
pass 1 "-% matches process environment"

run_img2sixel -% INVALID "${input_image}" > /dev/null 2>"${err_invalid}" && {
    fail 2 "invalid -% argument should fail"
    exit 0
}
pass 2 "invalid -% argument rejected"

exit 0
