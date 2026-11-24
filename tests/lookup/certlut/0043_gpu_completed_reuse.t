#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/certlut.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" certlut/gpu_completed_reuse || {
    printf '\n%s\n' "not ok 1 - certlut gpu completed reuse"
    exit 0
}
printf '\n%s\n' "ok 1 - certlut gpu completed reuse"
exit 0
