#!/bin/sh
# Coherent compatibility reads preserve signed and extended coordinates
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_extended_read" || {
    echo "not ok 1 - Coherent compatibility reads preserve signed and extended coordinates"
    exit 0
}
echo "ok 1 - Coherent compatibility reads preserve signed and extended coordinates"
exit 0
