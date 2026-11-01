#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Commit decoded GPU repair colors only at the bound plane origin.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0176_gpu_public_commit"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu public commit # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu public commit"
    exit 0
}
echo "ok 1 - gpu public commit"
exit 0
