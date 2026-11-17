#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0076_profile_header_missing_fields || {
    echo "not ok 1 - zero ICC header fields retain builtin compatibility"
    exit 0
}

echo "ok 1 - zero ICC header fields retain builtin compatibility"
exit 0
