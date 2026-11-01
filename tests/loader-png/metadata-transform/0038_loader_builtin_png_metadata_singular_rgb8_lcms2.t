#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png-source-precision.md
# Inspect PNG metadata singular RGB8 with lcms2

set -eux
set +e
output=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    SIXEL_LOADER_PREFER_8BIT=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0192_png_metadata_samples" 1 3 0 2>&1)
status=$?
set -e
printf '%s\n' "$output" >&2
test "$status" -ne 77 || {
    echo "1..0 # SKIP required CMS build path is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$status" -eq 0 || {
    echo "not ok 1 - Inspect PNG metadata singular RGB8 with lcms2"
    exit 0
}
case "${output}" in *"PNGMETA|evaluator="*) true ;; *) false ;; esac || {
    echo "not ok 1 - PNG metadata evaluator marker"
    exit 0
}
evaluator=${output##*"PNGMETA|evaluator="}
expected=${evaluator#*"|applies="}
evaluator=${evaluator%%|*}
count=0
first=${output#*"LSXCMS2|event=apply|"}
case "${output}" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac || count=1
second=${first#*"LSXCMS2|event=apply|"}
case "${first}" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac || count=2
{ case "${second}" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac && test "$expected" = "$count"; } || {
    echo "not ok 1 - PNG metadata apply count"
    exit 0
}
newline='
'
first=${first%%"$newline"*}
second=${second%%"$newline"*}
test "$count" -eq 0 || case "${first}" in engine="$evaluator"\|*) true ;; *) false ;; esac || {
    echo "not ok 1 - PNG metadata image evaluator"
    exit 0
}
test "$count" -eq 0 || case "${first}" in *\|ok=1) true ;; *) false ;; esac || {
    echo "not ok 1 - PNG metadata image application"
    exit 0
}
test "$count" -lt 2 || case "${second}" in engine="$evaluator"\|*) true ;; *) false ;; esac || {
    echo "not ok 1 - PNG metadata background evaluator"
    exit 0
}
test "$count" -lt 2 || case "${second}" in *\|ok=1) true ;; *) false ;; esac || {
    echo "not ok 1 - PNG metadata background application"
    exit 0
}
echo "ok 1 - Inspect PNG metadata singular RGB8 with lcms2"
exit 0
