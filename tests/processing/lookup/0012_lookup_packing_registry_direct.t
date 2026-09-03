#!/bin/sh
# Run the direct dense-packing registry contract test.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "lookup/0012_lookup_packing_registry_direct" || {
    echo "not ok 1 - direct dense packing registry contract"
    exit 0
}

echo "ok 1 - direct dense packing registry contract"
exit 0
