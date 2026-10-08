#!/bin/sh
# Verify refine gpu.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel disabled"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Gforce -dnone -p16 \
    -L 'builtin:cms_engine=none!' --precision=8bit -Xgamma -Wgamma \
    -aoff -Fnone '-~none' -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" || {
    echo "1..0 # SKIP GPU unavailable"
    exit 0
}

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/delta_native/0236_delta_refine_gpu || {
    echo "not ok 1 - refine_gpu"
    exit 0
}
echo "ok 1 - refine_gpu"
exit 0
