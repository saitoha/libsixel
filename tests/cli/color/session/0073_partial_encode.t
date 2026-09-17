#!/bin/sh
# Verify controller cancellation boundary: partial-encode.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job partial-encode || {
    echo "not ok 1 - color controller partial-encode"
    exit 0
}
echo "ok 1 - color controller partial-encode"
exit 0
