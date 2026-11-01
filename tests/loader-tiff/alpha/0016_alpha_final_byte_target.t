#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management/README.md
# Verify alpha final byte target with independent original samples.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
unset SIXEL_ALPHA_POLICY
set +x
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0320_alpha_final_byte_target "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - alpha final byte target"
    exit 0
}
# Require exactly one actual requested apply, or none for source fallback.
active=0
applies=0
completed=0
expected=none
while IFS= read -r line; do
    case "$line" in
    "TALPHA|load-begin|engine="*)
        test "$active" -eq 0 || { echo "not ok 1 - overlapping load"; exit 0; }
        active=1
        applies=0
        expected=${line##*=}
        ;;
    *"LSXCMS2|event=apply|"*)
        test "$active" -eq 1 || { echo "not ok 1 - unscoped apply"; exit 0; }
        case "$line" in
        *"|engine=$expected|"*"|ok=1") applies=$((applies + 1)) ;;
        *) echo "$trace" >&2; echo "not ok 1 - unexpected apply"; exit 0 ;;
        esac
        ;;
    "TALPHA|load-end|ok=1")
        wanted=1
        test "$expected" != none || wanted=0
        test "$active" -eq 1 && test "$applies" -eq "$wanted" || {
            echo "$trace" >&2
            echo "not ok 1 - incomplete engine observation"
            exit 0
        }
        active=0
        completed=$((completed + 1))
        ;;
    esac
done <<EOF
$trace
EOF
test "$active" -eq 0 && test "$completed" -eq 24 || {
    echo "not ok 1 - unexpected load count"
    exit 0
}
set -x
echo "ok 1 - alpha final byte target "
exit 0
