#!/bin/sh
# Policy: docs/writers/png.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0018_shared_rgb_p3" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0018_shared_rgb_p3" check || {
    echo "not ok 1 - shared_rgb_p3"
    exit 0
}
echo "ok 1 - shared_rgb_p3"
exit 0
