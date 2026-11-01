#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Borrowed ColorSync RGB profiles remain reusable after each temporary transform is deleted.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0128_colorsync_rgb_profile_reuse || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - colorsync rgb profile reuse"
    exit 0
}
echo "ok 1 - colorsync rgb profile reuse"
exit 0
