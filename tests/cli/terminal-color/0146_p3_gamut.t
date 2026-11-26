#!/bin/sh
# Check Display P3 negotiation preserves gamut mapping for Rec.2020 input.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 -a "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP img2sixel or libpng unavailable"
    set -v
    exit 0
}

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0039_cli_color_auto \
    p3-gamut "${IMG2SIXEL_PATH}" \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-rec2020-linear-primaries-16.png" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP scripted POSIX terminal I/O is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - negotiated P3 retains gamut mapping"
    exit 0
}
printf '\n%s\n' "ok 1 - negotiated P3 retains gamut mapping"
exit 0
