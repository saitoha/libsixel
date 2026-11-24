#!/bin/sh
# A limited 256-column run preserves its split and remainder.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0115_run_limited_256" || {
    printf '\n%s\n' "not ok 1 - run limited 256"
    exit 0
}
printf '\n%s\n' "ok 1 - run limited 256"
exit 0
