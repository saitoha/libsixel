#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management.md
# Verify legacy gray input allocation failure at the legacy helper and public frame boundaries.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
unset SIXEL_ALPHA_POLICY
set +x
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0378_legacy_gray_input_allocation_failure "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff, requested CMS or native helper is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - legacy gray input allocation failure"
    exit 0
}
active=0
applies=0
wanted=0
completed=0
expected=0
while IFS= read -r line; do
    case "$line" in
    "LC1|begin|"*)
        test "$active" -eq 0 || {
            echo "not ok 1 - overlapping CMS phase"
            exit 0
        }
        active=1
        applies=0
        wanted=${line##*=}
        ;;
    *"LSXCMS2|event=apply|"*)
        test "$active" -eq 1 || {
            echo "not ok 1 - unscoped CMS apply"
            exit 0
        }
        case "$line" in
        *"|engine=builtin|"*"|ok=1") applies=$((applies + 1)) ;;
        *) echo "$trace" >&2; echo "not ok 1 - wrong CMS backend or result"; exit 0 ;;
        esac
        ;;
    "LC1|end|ok=1")
        test "$active" -eq 1 && test "$applies" -eq "$wanted" || {
            echo "$trace" >&2
            echo "not ok 1 - unexpected CMS apply count"
            exit 0
        }
        active=0
        completed=$((completed + 1))
        ;;
    "LC1|complete|phases="*) expected=${line##*=} ;;
    esac
done <<EOF
$trace
EOF
test "$active" -eq 0 && test "$completed" -gt 0 &&
    test "$completed" -eq "$expected" || {
    echo "not ok 1 - incomplete CMS phases"
    exit 0
}
set -x
echo "ok 1 - legacy gray input allocation failure"
exit 0
