#!/bin/sh
# Verify the current artifact capabilities declared by each quantizer.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0033_quantizer_capabilities" || {
    printf '\n%s\n' "not ok 1 - quantizer capabilities"
    exit 0
}

printf '\n%s\n' "ok 1 - quantizer capabilities"
exit 0
