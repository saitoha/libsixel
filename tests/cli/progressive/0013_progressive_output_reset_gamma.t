#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify gamma replaces an output ICC before progressive repair.
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
    -.float32 -Wlinear -Goff -dnone -p4 '-~none' '-_none' \
    -Zprogressive:N2:M0 -Ugamma "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    printf '\n%s\n' 'not ok 1 - reference conversion failed'
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 '-Lbuiltin!' \
    -.float32 -Wlinear -Goff -dnone -p4 '-~none' '-_none' \
    -Zprogressive:N2:M0 -U "$profile" -Ugamma \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    printf '\n%s\n' 'not ok 1 - gamma replaces an output ICC before progressive repair'
    exit 0
}
test -n "$actual" && test "$actual" = "$reference" || {
    printf '\n%s\n' 'not ok 1 - gamma replaces an output ICC before progressive repair'
    exit 0
}
printf '\n%s\n' 'ok 1 - gamma replaces an output ICC before progressive repair'
exit 0
