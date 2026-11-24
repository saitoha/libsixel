#!/bin/sh
# Verify compose word boundaries.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0047_compose_word_boundaries" || {
    printf '\n%s\n' "not ok 1 - compose word boundaries"
    exit 0
}

printf '\n%s\n' "ok 1 - compose word boundaries"
exit 0
