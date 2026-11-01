#!/bin/sh
# fs CPU RGB preserves legacy stream bytes.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0030_fs_cpu_rgb_bytes" || {
    echo 'not ok 1 - 0030_fs_cpu_rgb_bytes'
    exit 0
}
echo 'ok 1 - 0030_fs_cpu_rgb_bytes'
exit 0
