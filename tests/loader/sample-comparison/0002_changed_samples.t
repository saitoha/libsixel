#!/bin/sh
# Reject changed RGB samples even when dimensions match.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm" \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_gray_expected.ppm" \
    'builtin!' 1.01 || status=$?
test "${status}" -eq 1 || {
    echo "not ok 1 - Reject changed RGB samples even when dimensions match"
    exit 0
}
echo "ok 1 - Reject changed RGB samples even when dimensions match"
exit 0
