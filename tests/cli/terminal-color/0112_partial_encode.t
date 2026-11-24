#!/bin/sh
# Verify controller cancellation boundary: partial-encode.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0040_cli_color_job partial-encode || {
    printf '\n%s\n' "not ok 1 - color controller partial-encode"
    exit 0
}
printf '\n%s\n' "ok 1 - color controller partial-encode"
exit 0
