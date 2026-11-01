#!/bin/sh
# Policy: docs/loader/builtin/tga.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify TGA gamma target using original codes and analytic values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=gamma \
    SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" tgac/0036_gamma_target_numeric 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - TGA gamma_target values"
    exit 0
}
case "$trace" in
    *"LSXCMS2|event=apply|engine=builtin|"*"|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - actual CMS apply missing"; exit 0 ;;
esac
echo "ok 1 - TGA gamma_target"
exit 0
