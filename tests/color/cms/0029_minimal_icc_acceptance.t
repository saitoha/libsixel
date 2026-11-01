#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Keep incomplete M0 profiles as builtin acceptance probes.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0024_minimal_icc_acceptance" || {
    echo "not ok 1 - incomplete M0 ICC builtin acceptance"
    exit 0
}
echo "ok 1 - incomplete M0 ICC builtin acceptance"
exit 0
