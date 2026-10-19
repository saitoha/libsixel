#!/bin/sh
# Gamma-only byte work must not select the extended palette migration.
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

trace=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L'libpng:cms_engine=none:cms_target=gamma:prefer_8bit=1!' \
    --precision=8bit -Qkmeans:seed=1 -Xgamma -Wgamma -Ugamma \
    -Fnone -aoff -_none '-~none' -dnone -trgb \
    --binning-policy=none --sampling-policy=full-frame \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-8-rgb.png" 2>&1) || {
    echo "$trace" >&2
    echo "not ok 1 - byte-eligible encode"
    exit 0
}
# Other diagnostic fields may follow rgb101 before the status codes.
case "$trace" in
    *'|rgb101=0|'*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - byte-eligible job selected extended palette"
        exit 0
        ;;
esac
echo "ok 1 - byte eligibility keeps legacy palette construction"
exit 0
