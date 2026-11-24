#!/bin/sh
# Verify the RGB range observer rejects a uniform neutral image.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_sample_range \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_gray_expected.ppm" 1 || status=$?
test "${status}" -eq 1 || {
    printf '\n%s\n' "not ok 1 - sample range control returned an unexpected status"
    exit 0
}
printf '\n%s\n' "ok 1 - RGB sample range control"
exit 0
