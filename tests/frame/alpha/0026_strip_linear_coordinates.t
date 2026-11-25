#!/bin/sh
# Stripping opaque alpha preserves linear coordinates until byte conversion.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical linear-strip || {
    printf '\n%s\n' 'not ok 1 - Stripping opaque alpha preserves linear coordinates until byte conversion'
    exit 0
}
printf '\n%s\n' 'ok 1 - Stripping opaque alpha preserves linear coordinates until byte conversion'
exit 0
