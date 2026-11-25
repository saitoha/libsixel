#!/bin/sh
# Reducing the declared palette size preserves retained storage.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical palette-recount || {
    printf '\n%s\n' 'not ok 1 - Reducing the declared palette size preserves retained storage'
    exit 0
}
printf '\n%s\n' 'ok 1 - Reducing the declared palette size preserves retained storage'
exit 0
