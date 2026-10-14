#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# A partial final band gathers only its valid rows and keeps zero opaque.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0057_ormode_tail_band" || {
    echo "not ok 1 - ormode_tail_band"
    exit 0
}

echo "ok 1 - ormode_tail_band"
exit 0
