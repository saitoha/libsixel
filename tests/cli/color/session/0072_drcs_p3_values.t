#!/bin/sh
# Verify explicit P3 DRCS values through a private scripted PTY.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/output/drcs.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 -a "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP img2sixel or libpng unavailable"
    set -v
    exit 0
}

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0039_cli_color_auto \
    drcs-p3 "${IMG2SIXEL_PATH}" \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-16-rgb.png" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP scripted POSIX terminal I/O is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - explicit P3 DRCS values"
    exit 0
}
echo "ok 1 - explicit P3 DRCS values"
exit 0
