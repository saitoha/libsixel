#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0026_gpu_resident_rgb_upload" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP GPU unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - Resident history avoids retained RGB upload'
    exit 0
}
echo 'ok 1 - Resident history avoids retained RGB upload'
exit 0
