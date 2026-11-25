#!/bin/sh
# TODO: Convert ICC RGB directly to extended float before linear compositing.
set -eux
echo '1..1'
set -v

status=0
trace=$(SIXEL_LOADER_PREFER_8BIT=0 SIXEL_LOADER_CMS_BPC=0 \
SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/red_alpha libwebp:Ebuiltin! \
    "${TOP_SRCDIR}/tests/data/color-numeric/red-alpha-p3.webp" 2>&1) || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP required backend unavailable'
    exit 0
}
printf '%s\n' "$trace" >&2
# Clipping and ignoring ICC both yield the same red over black. Require
# evidence of bounded builtin conversion. Builds with Little CMS support
# transform RGBA8 (3) directly. Other builds transform packed RGB8 (2)
# and copy back the color channels while preserving the original alpha.
case "$status:$trace" in
    42:*'LSXCMS2|event=apply|engine=builtin|path=bounded-device|src=2|dst=2|ok=1'*|\
    42:*'LSXCMS2|event=apply|engine=builtin|path=bounded-device|src=3|dst=3|ok=1'*)
        printf '\n%s\n' 'not ok 1 - WebP clips Display P3 red before alpha composition # TODO known result'
        exit 0 ;;
esac
test "$status" -ne 0 || {
    printf '\n%s\n' 'not ok 1 - expected behavior now works; promote this test'
    exit 0
}
printf '\n%s\n' 'not ok 1 - unexpected fixture, execution or pixel result'
exit 0
