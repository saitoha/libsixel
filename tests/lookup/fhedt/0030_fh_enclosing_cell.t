#!/bin/sh
# Verify fhedt enclosing cell.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0020_fh_enclosing_cell || {
    echo "not ok 1 - fhedt enclosing cell"
    exit 0
}

echo "ok 1 - fhedt enclosing cell"
exit 0
