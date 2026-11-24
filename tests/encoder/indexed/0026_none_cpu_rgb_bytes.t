#!/bin/sh
# none CPU RGB preserves legacy stream bytes.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0026_none_cpu_rgb_bytes" || {
    printf '\n%s\n' 'not ok 1 - 0026_none_cpu_rgb_bytes'
    exit 0
}
printf '\n%s\n' 'ok 1 - 0026_none_cpu_rgb_bytes'
exit 0
