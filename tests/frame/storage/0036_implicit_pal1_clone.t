#!/bin/sh
# An unspecified palette count preserves PAL1 storage and clone colors.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical implicit-palette || {
    printf '\n%s\n' 'not ok 1 - An unspecified palette count preserves PAL1 storage and clone colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - An unspecified palette count preserves PAL1 storage and clone colors'
exit 0
