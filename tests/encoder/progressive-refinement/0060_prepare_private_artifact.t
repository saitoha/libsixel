#!/bin/sh
# Verify prepare private artifact.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0060_prepare_private_artifact" || {
    printf '\n%s\n' "not ok 1 - prepare private artifact"
    exit 0
}
printf '\n%s\n' "ok 1 - prepare private artifact"
exit 0
