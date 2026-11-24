#!/bin/sh
# Run the direct dense-packing registry contract test.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "lookup/0012_lookup_packing_registry_direct" || {
    printf '\n%s\n' "not ok 1 - direct dense packing registry contract"
    exit 0
}

printf '\n%s\n' "ok 1 - direct dense packing registry contract"
exit 0
