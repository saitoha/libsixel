#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Workers retain the color selection inherited from an earlier band.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0060_ormode_inherited_register" || {
    echo "not ok 1 - inherited OR register"
    exit 0
}

echo "ok 1 - inherited OR register"
exit 0
