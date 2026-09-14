#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png-source-precision.md
# Inspect PNG metadata singular-filebg-16 with lcms2

set -eux
set +e
output=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    SIXEL_LOADER_PREFER_8BIT=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0192_png_metadata_samples" 35 3 0 2>&1)
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
    echo "not ok 1 - Inspect PNG metadata singular-filebg-16 with lcms2"
    exit 0
}
test "${output#*"PNGMETA|evaluator="}" != "$output" || {
    echo "not ok 1 - PNG metadata evaluator marker"
    exit 0
}
evaluator=${output##*"PNGMETA|evaluator="}
expected=${evaluator#*"|applies="}
evaluator=${evaluator%%|*}
count=0
first=${output#*"LSXCMS2|event=apply|"}
test "$first" = "$output" || count=1
second=${first#*"LSXCMS2|event=apply|"}
test "$second" = "$first" || count=2
third=${second#*"LSXCMS2|event=apply|"}
test "$third" = "$second" && test "$expected" = "$count" || {
    echo "not ok 1 - PNG metadata apply count"
    exit 0
}
newline='
'
first=${first%%"$newline"*}
second=${second%%"$newline"*}
test "$count" -eq 0 || test "${first#engine="$evaluator"|}" != "$first" || {
    echo "not ok 1 - PNG metadata image evaluator"
    exit 0
}
test "$count" -eq 0 || test "${first%|ok=1}" != "$first" || {
    echo "not ok 1 - PNG metadata image application"
    exit 0
}
test "$count" -lt 2 || test "${second#engine="$evaluator"|}" != "$second" || {
    echo "not ok 1 - PNG metadata background evaluator"
    exit 0
}
test "$count" -lt 2 || test "${second%|ok=1}" != "$second" || {
    echo "not ok 1 - PNG metadata background application"
    exit 0
}
echo "ok 1 - Inspect PNG metadata singular-filebg-16 with lcms2"
exit 0
