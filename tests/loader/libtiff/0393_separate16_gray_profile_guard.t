#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management.md
# Contiguous Gray16 rejects an RGB profile before apply through the shared checked helper.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
unset SIXEL_ALPHA_POLICY
set +x
result=0
trace=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0393_separate16_gray_profile_guard "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - separate16 gray profile guard"
    exit 0
}
echo "ok 1 - separate16 gray profile guard"
exit 0
