#!/bin/sh
# TAP test ensuring png: prefix writes to filesystem path.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

out_path="${ARTIFACT_LOCAL_DIR}/out.png"

run_sixel2png -o "png:${out_path}" <"${TOP_SRCDIR}/images/map8.six" || {
    echo "not ok" 1 "prefixed output command failed"
    exit 0
}

test -s "${out_path}" || {
    echo "not ok" 1 "prefixed output missing"
    exit 0
}

echo "ok" 1 "prefixed output trims scheme"
exit 0
