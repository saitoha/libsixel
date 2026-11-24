#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify strict boolean syntax and atomic rejection for progressive selection.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0157_progressive_boolean_syntax" || {
    printf '\n%s\n' "not ok 1 - progressive accepts only exact boolean 0 and 1"
    exit 0
}
printf '\n%s\n' "ok 1 - progressive accepts only exact boolean 0 and 1"
exit 0
