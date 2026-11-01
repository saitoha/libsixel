#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray8 builtin against analytic JPEG samples.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    bjicc/0001_bj_gray8_builtin_numeric 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - gray8 builtin values"
    exit 0
}
case "$trace" in *"event=apply|engine=lcms2|"*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS engine"
    exit 0 ;;
esac
case "$trace" in *"event=apply|engine=colorsync|"*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS engine"
    exit 0 ;;
esac
case "$trace" in
    *"LSXCMS2|event=apply|engine=builtin|"*"|src=9|dst=4|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - source-domain apply missing"; exit 0 ;;
esac
echo "ok 1 - gray8 builtin"
exit 0
