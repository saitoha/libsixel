#!/bin/sh
# Verify lossless WebP alpha composition in the published linear RGB basis.
set -eux
echo '1..1'
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0017_loader_libwebp_pixelformat \
    linear-alpha || result=$?
test "$result" -ne 77 || {
    echo 'ok 1 # SKIP libwebp unavailable'
    exit 0
}
test "$result" -eq 0 || {
    echo 'not ok 1 - libwebp publishes correct linear float32 alpha colors'
    exit 0
}
echo 'ok 1 - libwebp publishes correct linear float32 alpha colors'
exit 0
