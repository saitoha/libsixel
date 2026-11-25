#!/bin/sh
# Verify that replacing frame pixels resets the previous image's CMS history.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0239_alpha 17 || {
    printf '\n%s\n' 'not ok 1 - replacing frame pixels resets CMS and coverage state'
    exit 0
}
printf '\n%s\n' 'ok 1 - replacing frame pixels resets CMS and coverage state'
exit 0
