#!/bin/sh
# Verify config validation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0018_config_validation" || {
    echo "not ok 1 - config validation"
    exit 0
}
echo "ok 1 - config validation"
exit 0
