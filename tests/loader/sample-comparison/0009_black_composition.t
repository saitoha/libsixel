#!/bin/sh
# Accept independently computed black composition.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_black_values \
    "${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/basn6a08-srgb-black.ppm" \
    "${TOP_SRCDIR}/images/pngsuite/basic/basn6a08.png" \
    'builtin:cms_engine=none!' 0.004 || status=$?
test "${status}" -eq 0 || {
    echo "not ok 1 - Accept independently computed black composition"
    exit 0
}
echo "ok 1 - Accept independently computed black composition"
exit 0
