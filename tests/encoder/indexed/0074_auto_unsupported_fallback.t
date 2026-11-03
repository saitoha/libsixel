#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/auto_unsupported_fallback || {
    echo "not ok 1 - auto unsupported fallback"
    exit 0
}
echo "ok 1 - auto unsupported fallback"
exit 0
