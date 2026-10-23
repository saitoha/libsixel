#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0016_auto_dither" || {
    echo "not ok 1 - auto dither"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - auto dither (CLI unavailable)"
    exit 0
}
# Serial fallback does not attach the parallel producer timeline.
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "ok 1 - auto dither (parallel timeline unavailable)"
    exit 0
}
# Inspect regular files after the converters exit; shell device aliases do
# not necessarily name the same redirected stream in the native C runtime.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

SIXEL_LOG_PATH="${ARTIFACT_LOCAL_DIR}/lookup-none.jsonl" \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin:cms_engine=none! --threading-policy=8 --precision-policy=8bit -0hard \
    -w 64 -h 64 -d none '-~' auto -o/dev/null \
    "${TOP_SRCDIR}/images/snake.png" || {
    echo "not ok 1 - none CLI"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0016_auto_dither" \
    "${ARTIFACT_LOCAL_DIR}/lookup-none.jsonl" none || {
    echo "not ok 1 - none backend and dispatch timeline"
    exit 0
}

SIXEL_LOG_PATH="${ARTIFACT_LOCAL_DIR}/lookup-fs.jsonl" \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin:cms_engine=none! --threading-policy=8 --precision-policy=8bit -0hard \
    -w 64 -h 64 -d fs '-~' auto -o/dev/null \
    "${TOP_SRCDIR}/images/snake.png" || {
    echo "not ok 1 - fs CLI"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0016_auto_dither" \
    "${ARTIFACT_LOCAL_DIR}/lookup-fs.jsonl" fs || {
    echo "not ok 1 - fs backend and dispatch timeline"
    exit 0
}

SIXEL_LOG_PATH="${ARTIFACT_LOCAL_DIR}/lookup-lso2.jsonl" \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin:cms_engine=none! --threading-policy=8 --precision-policy=8bit -0hard \
    -w 64 -h 64 -d lso2 '-~' auto -o/dev/null \
    "${TOP_SRCDIR}/images/snake.png" || {
    echo "not ok 1 - lso2 CLI"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0016_auto_dither" \
    "${ARTIFACT_LOCAL_DIR}/lookup-lso2.jsonl" lso2 || {
    echo "not ok 1 - lso2 backend and dispatch timeline"
    exit 0
}
echo "ok 1 - auto dither"
exit 0
