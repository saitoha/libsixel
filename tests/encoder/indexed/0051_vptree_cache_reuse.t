#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/vptree_cache_reuse || {
    echo "not ok 1 - vptree cache reuse"
    exit 0
}
echo "ok 1 - vptree cache reuse"
exit 0
