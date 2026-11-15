#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify the builtin parser resource boundary.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0033_resource_alloc_boundary || {
    echo "not ok 1 - exactly 16 MiB of ICC tables is accepted"
    exit 0
}

echo "ok 1 - exactly 16 MiB of ICC tables is accepted"
exit 0
