#!/bin/sh
# Confirm lsqa -%/--env behaves like process environment variables.
set -eux

test "${HAVE_LSQA-}" = 1 || {
    printf "1..0 # SKIP lsqa is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..2"
set -v

reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
out_env="${ARTIFACT_LOCAL_DIR}/lsqa_env_ref.txt"
out_opt="${ARTIFACT_LOCAL_DIR}/lsqa_env_opt.txt"
err_invalid="${ARTIFACT_LOCAL_DIR}/lsqa_env_invalid.err"

run_lsqa --env SIXEL_THREADS=1 --env SIXEL_OPTION_PATH_SUGGESTIONS=0 \
    "${reference_image}" "${target_image}" >"${out_env}" || {
    fail 1 "reference environment assessment failed"
    exit 0
}

run_lsqa -% SIXEL_THREADS=1 -% SIXEL_OPTION_PATH_SUGGESTIONS=0 \
    "${reference_image}" "${target_image}" >"${out_opt}" || {
    fail 1 "-% assessment failed"
    exit 0
}

cmp -s "${out_env}" "${out_opt}" || {
    fail 1 "-% output differs from process environment"
    exit 0
}
pass 1 "-% matches process environment"

run_lsqa -% INVALID "${reference_image}" "${target_image}" > /dev/null \
    2>"${err_invalid}" && {
    fail 2 "invalid -% argument should fail"
    exit 0
}
pass 2 "invalid -% argument rejected"

exit 0
