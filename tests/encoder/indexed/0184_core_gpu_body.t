#!/bin/sh
# GPU band composition writes the same bytes as the CPU compose.
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
# Instrumented pipelines can require more threadgroup memory. The resulting
# body must still match the CPU bytes when Metal selects serial ordering.
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0184_core_gpu_body" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - core gpu body'
    exit 0
}
echo 'ok 1 - core gpu body'
exit 0
