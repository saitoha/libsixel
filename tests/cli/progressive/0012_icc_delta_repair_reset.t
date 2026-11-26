#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify delta P0 disables repair with an output ICC.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP img2sixel is disabled'
    exit 0
}

echo '1..1'
set -v

profile="icc:P${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-lut.icc"
set +x
reference=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -.float32 -Wsrgb-linear -Goff -dnone -p4 '-~none' '-_none' \
    -Zdelta:P0 -U "$profile" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    printf '\n%s\n' 'not ok 1 - reference conversion failed'
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -.float32 -Wsrgb-linear -Goff -dnone -p4 '-~none' '-_none' \
    -Zdelta:P1 -U "$profile" -Zdelta:P0 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    printf '\n%s\n' 'not ok 1 - delta P0 disables repair with an output ICC'
    exit 0
}
test -n "$actual" && test "$actual" = "$reference" || {
    printf '\n%s\n' 'not ok 1 - delta P0 disables repair with an output ICC'
    exit 0
}
printf '\n%s\n' 'ok 1 - delta P0 disables repair with an output ICC'
exit 0
