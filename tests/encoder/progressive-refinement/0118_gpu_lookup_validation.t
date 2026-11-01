#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Reject malformed lookup requests without publishing matches.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0168_gpu_lookup_validation"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu lookup validation # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu lookup validation"
    exit 0
}
echo "ok 1 - gpu lookup validation"
exit 0
