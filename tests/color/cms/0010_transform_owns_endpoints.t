#!/bin/sh
# Verify transform owns endpoints.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0004_transform_owns_endpoints" || {
    printf '\n%s\n' "not ok 1 - transform owns endpoints"
    exit 0
}

printf '\n%s\n' "ok 1 - transform owns endpoints"
exit 0
