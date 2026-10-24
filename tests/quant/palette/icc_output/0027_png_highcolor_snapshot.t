#!/bin/sh
# ICC snapshot numeric samples and profile.
# Policy: docs/encoder/output/output-colorspace.md
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
    -Xgamma -Wgamma -U "icc:path=${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" -trgb -I -o png:- - <<'PPM' | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output snapshot 1 unused
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
echo "ok 1 - PNG snapshot ICC snapshot"
exit 0
