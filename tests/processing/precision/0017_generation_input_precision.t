#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Initialization selects float or byte construction from its actual input.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0009_dither_promotion 8 1>&2 || {
    echo "not ok 1 - generation input precision"
    exit 0
}
echo "ok 1 - generation input precision"
exit 0
