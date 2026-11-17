#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0080_mpet_runtime_fallback || {
    echo "not ok 1 - runtime mpet evaluation failure preserves the matching legacy path"
    exit 0
}

echo "ok 1 - runtime mpet evaluation failure preserves the matching legacy path"
exit 0
