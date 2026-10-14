#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify presentation final no noise.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0149_presentation_final_no_noise" || {
    echo "not ok 1 - presentation final no noise"
    exit 0
}
echo "ok 1 - presentation final no noise"
exit 0
