#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0008_policy_atomicity || {
    echo "not ok 1 - policy atomicity"
    exit 0
}
echo "ok 1 - policy atomicity"
exit 0
