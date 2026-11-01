#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify para nan.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0010_para_nan || {
    echo "not ok 1 - para nan"
    exit 0
}
echo "ok 1 - para nan"
exit 0
