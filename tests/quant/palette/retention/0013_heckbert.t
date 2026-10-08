#!/bin/sh
# Compatibility Heckbert retains its typed byte-histogram export
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_heckbert" || {
    echo "not ok 1 - Compatibility Heckbert retains its typed byte-histogram export"
    exit 0
}
echo "ok 1 - Compatibility Heckbert retains its typed byte-histogram export"
exit 0
