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
test "${SIXEL_ENABLE_THREADS-0}" = 1 && test -w /dev/stderr || {
    echo "ok 1 - auto dither (parallel timeline stream unavailable)"
    exit 0
}
log=$(SIXEL_LOG_PATH=/dev/stderr ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin:cms_engine=none! --threads=8 --precision=8bit -0hard \
    -w 64 -h 64 -d none '-~' auto "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - none CLI"
    exit 0
}
case "$log" in
    *'"event":"lookup/vptree.8bit"'*) ;;
    *) echo "not ok 1 - none backend timeline"; exit 0 ;;
esac
case "$log" in
    *'"event":"none-batch"'*) ;;
    *) echo "not ok 1 - missing bulk mapping"; exit 0 ;;
esac

log=$(SIXEL_LOG_PATH=/dev/stderr ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin:cms_engine=none! --threads=8 --precision=8bit -0hard \
    -w 64 -h 64 -d fs '-~' auto "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - fs CLI"
    exit 0
}
case "$log" in
    *'"event":"lookup/vptree.8bit"'*) ;;
    *) echo "not ok 1 - fs backend timeline"; exit 0 ;;
esac
case "$log" in
    *'"event":"none-batch"'*)
        echo "not ok 1 - bulk mapping used for diffusion"; exit 0 ;;
esac

log=$(SIXEL_LOG_PATH=/dev/stderr ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin:cms_engine=none! --threads=8 --precision=8bit -0hard \
    -w 64 -h 64 -d lso2 '-~' auto "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - lso2 CLI"
    exit 0
}
case "$log" in
    *'"event":"lookup/vptree.8bit"'*) ;;
    *) echo "not ok 1 - lso2 backend timeline"; exit 0 ;;
esac
case "$log" in
    *'"event":"none-batch"'*)
        echo "not ok 1 - bulk mapping used for diffusion"; exit 0 ;;
esac
echo "ok 1 - auto dither"
exit 0
