#!/bin/sh
# Verify classify two color key.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0056_classify_two_color_key" || {
    printf '\n%s\n' "not ok 1 - classify two color key"
    exit 0
}

printf '\n%s\n' "ok 1 - classify two color key"
exit 0
