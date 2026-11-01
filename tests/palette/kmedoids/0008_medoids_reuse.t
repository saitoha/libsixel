#!/bin/sh
# A failed ordinary float build permits a clean subsequent attempt.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/medoids_reuse" || {
    echo "not ok 1 - A failed ordinary float build permits a clean subsequent attempt"
    exit 0
}
echo "ok 1 - A failed ordinary float build permits a clean subsequent attempt"
exit 0
