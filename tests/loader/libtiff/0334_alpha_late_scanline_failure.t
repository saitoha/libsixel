#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management/README.md
# Verify alpha late scanline failure with independent original samples.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
unset SIXEL_ALPHA_POLICY
set +x
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0334_alpha_late_scanline_failure "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - alpha late scanline failure"
    exit 0
}
# Separate native apply success from the later transactional postcondition.
active=0
applies=0
completed=0
applied=0
while IFS= read -r line; do
    case "$line" in
    "AFAULT|phase-begin="*)
        test "$active" -eq 0 || { echo "not ok 1 - overlapping phase"; exit 0; }
        active=1
        applies=0
        applied=0
        ;;
    *"LSXCMS2|event=apply|"*)
        test "$active" -eq 1 || { echo "not ok 1 - unscoped apply"; exit 0; }
        case "$line" in
        *"|engine=builtin|"*) applies=$((applies + 1)); applied=${line##*=} ;;
        *) echo "$trace" >&2; echo "not ok 1 - wrong engine"; exit 0 ;;
        esac
        ;;
    "AFAULT|phase="*)
        wanted=1
        success=1
        case "$line" in
        "AFAULT|phase=1|mode=1|"*) success=0 ;;
        "AFAULT|phase=1|mode=3|"*) success=1 ;;
        "AFAULT|phase=1|"*) wanted=0; success=0 ;;
        esac
        test "$active" -eq 1 && test "$applies" -eq "$wanted" &&
            test "$applied" -eq "$success" || {
            echo "$trace" >&2
            echo "not ok 1 - incomplete phase apply observation"
            exit 0
        }
        active=0
        completed=$((completed + 1))
        ;;
    esac
done <<EOF
$trace
EOF
test "$active" -eq 0 && test "$completed" -eq 9 || {
    echo "not ok 1 - unexpected phase count"
    exit 0
}
set -x
echo "ok 1 - alpha late scanline failure "
exit 0
