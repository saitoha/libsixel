#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0017_legacy_writer_untagged" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0017_legacy_writer_untagged" check || {
    printf '\n%s\n' "not ok 1 - legacy_writer_untagged"
    exit 0
}
printf '\n%s\n' "ok 1 - legacy_writer_untagged"
exit 0
