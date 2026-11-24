#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify non gamma float work on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0004_non_gamma_float_work 1>&2 || {
    printf '\n%s\n' "not ok 1 - non gamma float work"
    exit 0
}
printf '\n%s\n' "ok 1 - non gamma float work"
exit 0
