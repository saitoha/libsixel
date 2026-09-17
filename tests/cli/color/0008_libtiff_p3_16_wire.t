#!/bin/sh
# Verify exact Display P3 RGB101 colors from the selected libtiff path.
# Policy: docs/functionality/output-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    set -v
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    echo "1..0 # SKIP libtiff unavailable"
    set -v
    exit 0
}

echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L"libtiff:cms_engine=builtin:cms_target=linear:prefer_8bit=0!" \
    -jscalar:resize_precision=float --precision=float -Qkmeans:seed=1 \
    -Xlinear -Wlinear -Udisplay-p3 -Fnone -aoff -_none '-~none' -dnone -trgb \
    --binning-policy=none --sampling-policy=full-frame \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-layout-rgb16-o6.tif" 2>&1) || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff_p3_16_wire conversion"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    libtiff-p3 "$trace" || {
    echo "not ok 1 - libtiff_p3_16_wire independent values or structure"
    exit 0
}

echo "ok 1 - libtiff_p3_16_wire independent RGB101 values and structure"
exit 0
