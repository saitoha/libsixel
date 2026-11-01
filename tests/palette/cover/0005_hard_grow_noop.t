#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# Hard mode and growth do not enable AUTO repair.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0005_hard_grow_noop || {
    echo "not ok 1 - Hard mode and growth do not enable AUTO repair"
    exit 0
}
echo "ok 1 - Hard mode and growth do not enable AUTO repair"
exit 0
