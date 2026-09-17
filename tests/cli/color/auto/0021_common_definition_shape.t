#!/bin/sh
# Verify terminal color parser common definition shape.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0037_cli_color_parser \
    common-shape || {
    echo "not ok 1 - terminal color parser common definition shape"
    exit 0
}

echo "ok 1 - terminal color parser common definition shape"
exit 0
