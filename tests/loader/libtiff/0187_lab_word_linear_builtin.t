#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify lab word linear builtin.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0187_lab_word_linear_builtin \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-lab-word-521.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - lab word linear builtin"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=builtin|"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - requested CMS did not apply"
    exit 0
}
test "${trace#*"|src=8|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - Lab to float RGB apply was not observed"
    exit 0
}
test "${trace#*"LSXLOAD1|event=ok|loader=libtiff|"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the linear frame"
    exit 0
}
echo "ok 1 - lab word linear builtin"
exit 0
