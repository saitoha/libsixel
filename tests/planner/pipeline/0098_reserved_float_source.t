#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Reserved sRGB colors survive linear float32 encoding.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0098_reserved_float_source" || {
    echo "not ok 1 - reserved float source"
    exit 0
}
echo "ok 1 - reserved float source"
exit 0
