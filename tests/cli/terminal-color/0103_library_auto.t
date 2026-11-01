#!/bin/sh
# Verify that the library rejects CLI-only automatic negotiation.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job library-auto || {
    echo "not ok 1 - color controller library-auto"
    exit 0
}
echo "ok 1 - color controller library-auto"
exit 0
