#!/bin/sh
# Reject different image dimensions before comparing samples.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm" \
    "${TOP_SRCDIR}/tests/data/color-numeric/red.six" \
    'builtin!' 1.01 || status=$?
test "${status}" -eq 1 || {
    echo "not ok 1 - Reject different image dimensions before comparing samples"
    exit 0
}
echo "ok 1 - Reject different image dimensions before comparing samples"
exit 0
