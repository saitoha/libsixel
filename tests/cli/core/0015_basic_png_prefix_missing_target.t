#!/bin/sh
# TAP test ensuring an empty png: prefix is rejected.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

png_err="${ARTIFACT_LOCAL_DIR}/sixel2png-png-err"

run_sixel2png -o "png:" <"${TOP_SRCDIR}/images/map8.six" \
        >"${ARTIFACT_LOCAL_DIR}/capture.$$" 2>"${png_err}" && {
    fail 1 "accepts empty png: prefix"
    exit 0
}

grep 'missing target after the "png:" prefix' "${png_err}" >/dev/null || {
    fail 1 "missing png prefix diagnostic"
    exit 0
}

pass 1 "rejects empty png prefix"
exit 0
