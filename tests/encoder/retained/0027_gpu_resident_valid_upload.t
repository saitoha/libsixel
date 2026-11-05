#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0027_gpu_resident_valid_upload" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP GPU unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - Resident history avoids validity upload'
    exit 0
}
echo 'ok 1 - Resident history avoids validity upload'
exit 0
