#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify the builtin parser resource boundary.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0031_resource_tag_boundary || {
    echo "not ok 1 - 256 ICC tags are accepted"
    exit 0
}

echo "ok 1 - 256 ICC tags are accepted"
exit 0
