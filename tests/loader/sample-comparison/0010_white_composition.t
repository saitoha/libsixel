#!/bin/sh
# Accept independently computed white composition.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_white_values \
    "${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/bgwn6a08-srgb-white.ppm" \
    "${TOP_SRCDIR}/images/pngsuite/background/bgwn6a08.png" \
    'builtin:cms_engine=none!' 0.004 || status=$?
test "${status}" -eq 0 || {
    echo "not ok 1 - Accept independently computed white composition"
    exit 0
}
echo "ok 1 - Accept independently computed white composition"
exit 0
