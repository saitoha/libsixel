#!/bin/sh
# Verify named xyz.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0001_named_xyz" || {
    echo "not ok 1 - named xyz"
    exit 0
}

echo "ok 1 - named xyz"
exit 0
