#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0019_raw_decode_unchanged" || {
    printf '\n%s\n' "not ok 1 - raw_decode_unchanged"
    exit 0
}
printf '\n%s\n' "ok 1 - raw_decode_unchanged"
exit 0
