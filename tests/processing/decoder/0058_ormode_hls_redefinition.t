#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# OR resolves HLS definitions after painting, including palette zero.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0058_ormode_hls_redefinition" || {
    echo "not ok 1 - ormode_hls_redefinition"
    exit 0
}

echo "ok 1 - ormode_hls_redefinition"
exit 0
