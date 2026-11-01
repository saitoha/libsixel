#!/bin/sh
# Verify the current artifact capabilities declared by each quantizer.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0033_quantizer_capabilities" || {
    echo "not ok 1 - quantizer capabilities"
    exit 0
}

echo "ok 1 - quantizer capabilities"
exit 0
