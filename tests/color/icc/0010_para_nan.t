#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify para nan.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0010_para_nan || {
    printf '\n%s\n' "not ok 1 - para nan"
    exit 0
}
printf '\n%s\n' "ok 1 - para nan"
exit 0
