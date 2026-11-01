#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management/README.md
# Gray CMS packing allocation failure preserves every original source word and recovers.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
unset SIXEL_ALPHA_POLICY
set +x
result=0
trace=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0404_separate16_gray_pack_allocation "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - separate16 gray pack allocation"
    exit 0
}
echo "ok 1 - separate16 gray pack allocation"
exit 0
