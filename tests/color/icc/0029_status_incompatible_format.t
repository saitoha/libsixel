#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify that apply reports format mismatch and success.

set -eux

echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    icc/0029_status_incompatible_format 2>&1) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' "not ok 1 - apply reports format mismatch and success"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|engine=builtin|"*"|ok=0"*"LSXCMS2|event=transform-failure|engine=builtin|src=1|dst=4|reason=incompatible-format"*"|ok=1"*) true ;; *) false ;; esac || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' "not ok 1 - apply reports format mismatch and success"
    exit 0
}

printf '\n%s\n' "ok 1 - apply reports format mismatch and success"
exit 0
