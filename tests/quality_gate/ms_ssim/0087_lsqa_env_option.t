#!/bin/sh
# Confirm lsqa -%/--env behaves like process environment variables.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
out_env="${ARTIFACT_LOCAL_DIR}/lsqa_env_ref.txt"
out_opt="${ARTIFACT_LOCAL_DIR}/lsqa_env_opt.txt"

run_lsqa --env SIXEL_THREADS=1 --env SIXEL_OPTION_PATH_SUGGESTIONS=0 \
    "${reference_image}" "${target_image}" >"${out_env}" || {
    echo "not ok" 1 "reference environment assessment failed"
    exit 0
}

run_lsqa -% SIXEL_THREADS=1 -% SIXEL_OPTION_PATH_SUGGESTIONS=0 \
    "${reference_image}" "${target_image}" >"${out_opt}" || {
    echo "not ok" 1 "-% assessment failed"
    exit 0
}

cmp -s "${out_env}" "${out_opt}" || {
    echo "not ok" 1 "-% output differs from process environment"
    exit 0
}

run_lsqa -% INVALID "${reference_image}" "${target_image}" > /dev/null && {
    echo "not ok" 1 "invalid -% argument should fail"
    exit 0
}
echo "ok" 1 "invalid -% argument rejected"

exit 0
