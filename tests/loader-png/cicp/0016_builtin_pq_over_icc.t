#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# builtin pq over icc: numeric samples, storage, and alpha.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=srgb-linear \
    SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cicp/0016_builtin_pq_over_icc 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS backend unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    printf '\n%s\n' "not ok 1 - builtin pq over icc"
    exit 0
}
case "$trace" in
    *"PNGHDR1|source=display-p3-pq|"*) ;;
    *) echo "$trace" >&2; printf '\n%s\n' "not ok 1 - CMS apply missing"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - builtin pq over icc"
exit 0
