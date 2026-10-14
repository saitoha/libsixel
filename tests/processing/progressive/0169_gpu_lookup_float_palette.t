#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Search ordered signed float palettes with first-slot ties.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0169_gpu_lookup_float_palette"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu lookup float palette # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu lookup float palette"
    exit 0
}
echo "ok 1 - gpu lookup float palette"
exit 0
