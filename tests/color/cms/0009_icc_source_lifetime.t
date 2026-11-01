#!/bin/sh
# Verify icc source lifetime.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0003_icc_source_lifetime" || {
    echo "not ok 1 - icc source lifetime"
    exit 0
}

echo "ok 1 - icc source lifetime"
exit 0
