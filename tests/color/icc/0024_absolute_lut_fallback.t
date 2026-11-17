#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Use the next requested intent when absolute has no colorimetric path.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0024_absolute_lut_fallback || {
    echo "not ok 1 - unavailable absolute leaves saturation available"
    exit 0
}

echo "ok 1 - unavailable absolute leaves saturation available"
exit 0
