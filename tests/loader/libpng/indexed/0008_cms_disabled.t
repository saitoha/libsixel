#!/bin/sh
# Policy: docs/loader/libpng.md
# Keep original palette bytes and indices with CMS disabled.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/libpng_indexed_precision 8 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - Keep original palette bytes and indices with CMS disabled"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected ICC transform"
    exit 0
}
echo "ok 1 - Keep original palette bytes and indices with CMS disabled"
exit 0
