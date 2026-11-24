#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify default off against an algebraic reference.
set -eux
echo '1..1'
set -v
unset SIXEL_LOADER_CMS_BPC
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    bpc/0001_default_off || {
    printf '\n%s\n' 'not ok 1 - default off'
    exit 0
}
printf '\n%s\n' 'ok 1 - default off'
exit 0
