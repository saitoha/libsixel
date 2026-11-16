#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0038_mpet_unknown_fallback || {
    echo "not ok 1 - an unknown mpet element leaves the legacy transform usable"
    exit 0
}

echo "ok 1 - an unknown mpet element leaves the legacy transform usable"
exit 0
