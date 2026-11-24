#!/bin/sh
# Verify terminal color parser simple definition invalid.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    simple-invalid || {
    printf '\n%s\n' "not ok 1 - terminal color parser simple definition invalid"
    exit 0
}

printf '\n%s\n' "ok 1 - terminal color parser simple definition invalid"
exit 0
