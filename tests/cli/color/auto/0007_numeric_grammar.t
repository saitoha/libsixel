#!/bin/sh
# Verify terminal color parser numeric grammar.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    numeric || {
    echo "not ok 1 - terminal color parser numeric grammar"
    exit 0
}

echo "ok 1 - terminal color parser numeric grammar"
exit 0
