#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify trusted literal-run edge clipping in normal and OR mode.
set -eux
echo "1..1"
set -v

SIXEL_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0100_trusted_literal_run_edges" || {
    echo "not ok 1 - trusted literal run edges"
    exit 0
}
echo "ok 1 - trusted literal run edges"
exit 0
