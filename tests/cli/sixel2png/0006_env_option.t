#!/bin/sh
# Confirm sixel2png -%/--env behaves like process environment variables.
set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..2"
set -v

input_six="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
out_env="${ARTIFACT_LOCAL_DIR}/sixel2png_env_ref.png"
out_opt="${ARTIFACT_LOCAL_DIR}/sixel2png_env_opt.png"
err_invalid="${ARTIFACT_LOCAL_DIR}/sixel2png_env_invalid.err"

run_sixel2png --env SIXEL_THREADS=1 --env SIXEL_OPTION_PATH_SUGGESTIONS=0 \
    -i "${input_six}" -o "${out_env}" || {
    fail 1 "reference environment decode failed"
    exit 0
}

run_sixel2png -% SIXEL_THREADS=1 -% SIXEL_OPTION_PATH_SUGGESTIONS=0 \
    -i "${input_six}" -o "${out_opt}" || {
    fail 1 "-% decode failed"
    exit 0
}

cmp -s "${out_env}" "${out_opt}" || {
    fail 1 "-% output differs from process environment"
    exit 0
}
pass 1 "-% matches process environment"

run_sixel2png -% INVALID -i "${input_six}" > /dev/null 2>"${err_invalid}" && {
    fail 2 "invalid -% argument should fail"
    exit 0
}
pass 2 "invalid -% argument rejected"

exit 0
