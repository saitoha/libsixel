#!/bin/sh
# Verify highcolor byte define through the actual hexadecimal callback.
# Policy: docs/encoder/output/terminal-macros.md
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/macro/0023_highcolor_byte_define "${ARTIFACT_LOCAL_DIR}" || {
    printf '\n%s\n' "not ok 1 - highcolor byte define"
    exit 0
}
printf '\n%s\n' "ok 1 - highcolor byte define"
exit 0
