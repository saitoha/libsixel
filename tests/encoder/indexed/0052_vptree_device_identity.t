#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/vptree_device_identity || {
    echo "not ok 1 - vptree device identity"
    exit 0
}
echo "ok 1 - vptree device identity"
exit 0
