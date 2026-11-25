#!/bin/sh
set -eux
echo '1..1'
set -v

test -n "${SIXEL_WEBGPU_TEST_CLI-}" || {
    printf '\n%s\n' 'ok 1 # SKIP requires a thread-disabled WebGPU img2sixel build'
    exit 0
}
command -v node >/dev/null || {
    printf '\n%s\n' 'ok 1 # SKIP Node.js is unavailable'
    exit 0
}
node "${TOP_SRCDIR}/tests/platform/webgpu/node-runner.mjs" \
    "${SIXEL_WEBGPU_TEST_CLI}" autoOutputFallback || {
    printf '\n%s\n' 'not ok 1 - Node.js could not run the WebGPU test'
    exit 0
}
exit 0
