#!/bin/sh
# Policy: docs/writers/png.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0019_raw_decode_unchanged" || {
    echo "not ok 1 - raw_decode_unchanged"
    exit 0
}
echo "ok 1 - raw_decode_unchanged"
exit 0
