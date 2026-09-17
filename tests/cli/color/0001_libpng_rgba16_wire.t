#!/bin/sh
# Verify independent RGB101 values and geometry through the actual libpng CLI.
# Policy: docs/loader/libpng.md
# Policy: docs/loader/color-management.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    set -v
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng unavailable"
    set -v
    exit 0
}

echo "1..1"
set -v

# One CLI invocation owns the conversion. The checker reads its captured wire
# and actual loader/CMS records; it never launches a second image conversion.
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L"libpng:cms_engine=builtin:cms_target=linear:prefer_8bit=0!" \
    -jscalar:resize_precision=float --precision=float -aoff -Qheckbert \
    -Xgamma -Wgamma -Ugamma -Fnone -_none '-~none' -dnone -trgb \
    -Akeep -Bblack -w8 -h4 -rnearest \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-16-rgba.png" 2>&1) || {
    echo "$trace" >&2
    echo "not ok 1 - libpng CLI conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    libpng "$trace" || {
    echo "not ok 1 - libpng CLI wire values or structure"
    exit 0
}

echo "ok 1 - libpng CLI independent RGB101 values and structure"
exit 0
