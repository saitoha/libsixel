#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 && test "${HAVE_LIBTIFF-}" = 1 || {
    echo "ok 1 # SKIP img2sixel or libtiff disabled"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L'libtiff:cms_engine=builtin:cms_target=linear:prefer_8bit=0!' \
    --threading-policy=1 -.auto -Qheckbert -Fnone -aoff -_none '-~none' -dnone \
    -Xlinear -Wlinear -Udci-p3 -trgb -p2 \
    "${TOP_SRCDIR}/tests/data/color-numeric/named-output-p3.tif" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0040_dci_libtiff || {
    echo "not ok 1 - P3 libtiff input to dci-p3"
    exit 0
}
echo "ok 1 - P3 libtiff input to dci-p3"
exit 0
