#!/bin/sh
# Verify gamma working coordinates and automatic P3 precision.
# Policy: docs/encoder/output/output-colorspace.md
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

trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=balanced:global_budget=1 -L"libpng:cms_engine=builtin:cms_target=linear:prefer_8bit=0!" \
    -jscalar:resize_precision=float --precision-policy=auto -Qkmeans:seed=1 \
    -Xlinear -Wgamma -Udisplay-p3 -Fnone -aoff -_none '-~none' -dnone -trgb \
    --binning-policy=none --sampling-policy=full-frame \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-8-rgb.png" 2>&1) || {
    echo "$trace" >&2
    echo "not ok 1 - libpng_p3_gamma_auto_wire conversion"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    libpng-p3-8 "$trace" || {
    echo "not ok 1 - libpng_p3_gamma_auto_wire independent values or structure"
    exit 0
}

echo "ok 1 - libpng_p3_gamma_auto_wire independent RGB101 values and structure"
exit 0
