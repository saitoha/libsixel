#!/bin/sh
# Verify compose band reset.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0048_compose_band_reset" || {
    echo "not ok 1 - compose band reset"
    exit 0
}

echo "ok 1 - compose band reset"
exit 0
