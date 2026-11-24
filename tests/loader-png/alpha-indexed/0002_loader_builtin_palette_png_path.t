#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP test confirming builtin loader keeps indexed PNG palette path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/palette.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_png}" >/dev/null || {
    printf '\n%s\n' "not ok 1 - builtin loader indexed PNG palette path failed"
    exit 0
}

printf '\n%s\n' "ok 1 - builtin loader keeps indexed PNG palette path"
exit 0
