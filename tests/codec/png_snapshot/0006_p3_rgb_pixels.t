#!/bin/sh
# PNG snapshot p3 rgb pixels.
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 # SKIP img2sixel disabled"
    exit 0
}
set +e
SIXEL_DECODER_INPUT_COLORSPACE=srgb \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! --cms-engine=none \
    --threading-policy=balanced:global_budget=1 -.f -Qheckbert -Fnone -aoff -_none '-~none' -dnone \
    -Xgamma -Wgamma -Udisplay-p3 -trgb -p3 -o png:- - <<'PPM' | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/png_snapshot/0006_p3_rgb_pixels
P3
3 1
255
255 0 0  0 255 0  0 0 255
PPM
test "$?" = 0 || {
    echo "not ok 1 - PNG snapshot pixels and profile"
    exit 0
}
set -e
echo "ok 1 - PNG snapshot p3 rgb pixels"
exit 0
