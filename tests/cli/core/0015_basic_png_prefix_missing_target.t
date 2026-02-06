#!/bin/sh
# TAP test ensuring an empty png: prefix is rejected.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

png_err=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "sixel2png-png-err")

run_sixel2png -o "png:" <"${images_dir}/snake.six" \
        >"${ARTIFACT_LOCAL_DIR}/capture.$$" 2>"${png_err}" && {
    fail 1 "accepts empty png: prefix"
    exit 0
}

grep -F 'missing target after the "png:" prefix' "${png_err}" >/dev/null || {
    fail 1 "missing png prefix diagnostic"
    exit 0
}

pass 1 "rejects empty png prefix"
exit 0
