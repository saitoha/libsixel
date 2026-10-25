#!/bin/sh
# Verify request owns settings and endpoints.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0027_palette_request_ownership || {
    echo "not ok 1 - request owns settings and endpoints"
    exit 0
}
echo "ok 1 - request owns settings and endpoints"
exit 0
