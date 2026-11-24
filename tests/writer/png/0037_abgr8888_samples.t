#!/bin/sh
# Check RGB order and transparency when writing ABGR8888 samples.
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" writer/png/0037_abgr8888_samples \
    "${ARTIFACT_LOCAL_DIR}/packed.png" || {
    printf '\n%s\n' "not ok 1 - PNG ABGR8888 samples"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG ABGR8888 samples"
exit 0
