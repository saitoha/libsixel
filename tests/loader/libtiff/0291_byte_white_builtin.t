#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management.md
# Verify byte white builtin with independent source values.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
# Keep successful trace assertions compact; print the trace on failure.
set +x
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0291_byte_white_builtin "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - byte white builtin"
    exit 0
}
# Walk each line once and require one successful requested apply per load.
active=0
applies=0
completed=0
plain=0
background=0
while IFS= read -r line; do
    case "$line" in
    "TBYTE|load-begin|background=0")
        test "$active" -eq 0 || {
            echo "not ok 1 - overlapping load observations"
            exit 0
        }
        active=1
        applies=0
        plain=$((plain + 1))
        ;;
    "TBYTE|load-begin|background=1")
        test "$active" -eq 0 || {
            echo "not ok 1 - overlapping background observations"
            exit 0
        }
        active=1
        applies=0
        background=$((background + 1))
        ;;
    *"LSXCMS2|event=apply|engine=builtin|"*"|ok=1")
        test "$active" -eq 1 || {
            echo "not ok 1 - apply outside the observed load"
            exit 0
        }
        applies=$((applies + 1))
        ;;
    *"LSXCMS2|event=apply|"*)
        echo "$line" >&2
        echo "not ok 1 - unexpected engine or failed apply"
        exit 0
        ;;
    "TBYTE|load-end|ok=1")
        test "$active" -eq 1 && test "$applies" -eq 1 || {
            echo "not ok 1 - load lacks exactly one requested successful apply"
            exit 0
        }
        active=0
        completed=$((completed + 1))
        ;;
    esac
done <<EOF
$trace
EOF
test "$active" -eq 0 && test "$completed" -eq 2 && \
    test "$plain" -eq 1 && test "$background" -eq 1 || {
    echo "not ok 1 - incomplete ordinary or background apply observations"
    exit 0
}
set -x
echo "ok 1 - byte white builtin "
exit 0
