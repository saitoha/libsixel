#!/bin/sh
# smptec output FS propagates retained-color error in gamma working units.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0132_delta_smptec_output_diffuse || {
    echo "not ok 1 - delta smptec output diffuse"
    exit 0
}
echo "ok 1 - delta smptec output diffuse"
exit 0
