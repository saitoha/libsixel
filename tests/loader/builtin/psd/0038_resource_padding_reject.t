#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify resource padding reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0038_resource_padding_reject 1>&2 || {
    echo "not ok 1 - resource padding reject"
    exit 0
}
echo "ok 1 - resource padding reject"
exit 0
