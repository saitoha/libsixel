#!/bin/sh
# Verify binding bounds.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0022_binding_bounds" || {
    printf '\n%s\n' "not ok 1 - binding bounds"
    exit 0
}
printf '\n%s\n' "ok 1 - binding bounds"
exit 0
