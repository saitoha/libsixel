#!/bin/sh
# An all-masked frame declares its extent without painting.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0062_mask_all || {
    echo "not ok 1 - high-color mask all"
    exit 0
}
echo "ok 1 - high-color mask all"
exit 0
