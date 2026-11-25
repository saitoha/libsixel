#!/bin/sh
# Failed byte resize preserves linear float samples for a retry.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical linear-resize-retry || {
    printf '\n%s\n' 'not ok 1 - Failed byte resize preserves linear float samples for a retry'
    exit 0
}
printf '\n%s\n' 'ok 1 - Failed byte resize preserves linear float samples for a retry'
exit 0
