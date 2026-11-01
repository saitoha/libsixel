#!/bin/sh
# Verify delta byte syntax reuse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0072_delta_byte_syntax_reuse || {
    echo "not ok 1 - delta byte syntax reuse"
    exit 0
}
echo "ok 1 - delta byte syntax reuse"
exit 0
