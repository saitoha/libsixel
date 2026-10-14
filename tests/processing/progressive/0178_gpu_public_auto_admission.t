#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Admit AUTO by selected population independently of dispatch size.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0178_gpu_public_auto_admission"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu public auto admission # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu public auto admission"
    exit 0
}
echo "ok 1 - gpu public auto admission"
exit 0
