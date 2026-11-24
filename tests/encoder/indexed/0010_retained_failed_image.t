#!/bin/sh
# A failed producer cannot commit an accepted stream.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0010_retained_failed_image" || {
    printf '\n%s\n' 'not ok 1 - retained_failed_image'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained_failed_image'
exit 0
