#!/bin/sh
# Verify linear palette rejects Display P3.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/0003_default_colorspace_auto \
    linear-palette 3 >&2 || {
    printf '\n%s\n' 'not ok 1 - linear palette rejects Display P3'
    exit 0
}
printf '\n%s\n' 'ok 1 - linear palette rejects Display P3'
exit 0
