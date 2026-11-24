#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0018_shared_rgb_p3" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0018_shared_rgb_p3" check || {
    printf '\n%s\n' "not ok 1 - shared_rgb_p3"
    exit 0
}
printf '\n%s\n' "ok 1 - shared_rgb_p3"
exit 0
