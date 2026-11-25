#!/bin/sh
# TODO: Carry the decoded ICC history through canvas publication and replay.
# Preserve actual converted pixels and never infer history from file tags.
set -eux
echo '1..1'
set -v

status=0
trace=$(SIXEL_LOADER_PREFER_8BIT=0 SIXEL_LOADER_CMS_BPC=0 \
SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=apng_decode \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/red_alpha libpng:Ebuiltin! \
    "${TOP_SRCDIR}/tests/data/color-numeric/red-alpha-p3.apng" 2>&1) || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP required backend unavailable'
    exit 0
}
printf '%s\n' "$trace" >&2
case "$trace" in
    *'stop loop_no=2 frames_in_loop=2 num_plays=2 (cache)'*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - libpng did not replay the second APNG play from cache'
    exit 0
}
test "$status" -ne 42 || {
    printf '\n%s\n' 'not ok 1 - libpng drops ICC history across two APNG plays # TODO known result'
    exit 0
}
test "$status" -ne 0 || {
    printf '\n%s\n' 'not ok 1 - expected behavior now works; promote this test'
    exit 0
}
printf '\n%s\n' 'not ok 1 - unexpected fixture, execution or pixel result'
exit 0
