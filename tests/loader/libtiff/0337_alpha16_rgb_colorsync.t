#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management/README.md
# Verify alpha16 rgb colorsync with independent original samples.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
unset SIXEL_ALPHA_POLICY
set +x
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0337_alpha16_rgb_colorsync "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - alpha16 rgb colorsync"
    exit 0
}
# Require exact native applies; two Gray groups are not a retry.
active=0
applies=0
completed=0
expected=none
wanted=0
groups=0
seen=0
first=
second=
while IFS= read -r line; do
    case "$line" in
    "TALPHA|load-begin|engine="*)
        test "$active" -eq 0 || { echo "not ok 1 - overlapping load"; exit 0; }
        active=1
        applies=0
        expected=${line##*=}
        groups=0
        seen=0
        first=
        second=
        ;;
    "TALPHA16|applies="*)
        wanted=${line#*applies=}
        wanted=${wanted%%|*}
        groups=${line##*=}
        ;;
    "TALPHA16|source="*)
        case "$first" in
        "") first=${line##*=} ;;
        *) second=${line##*=} ;;
        esac
        ;;
    *"LSXCMS3|event=gamma-source|samples="*)
        case "$groups" in
        1|2)
            observed=${line##*=}
            wanted_source=$first
            test "$seen" -eq 0 || wanted_source=$second
            test "$observed" = "$wanted_source" || {
                echo "$trace" >&2
                echo "not ok 1 - native source words changed"
                exit 0
            }
            seen=$((seen + 1))
            ;;
        esac
        ;;
    *"LSXCMS2|event=apply|"*)
        test "$active" -eq 1 || { echo "not ok 1 - unscoped apply"; exit 0; }
        case "$line" in
        *"|engine=$expected|"*"|ok=1") applies=$((applies + 1)) ;;
        *) echo "$trace" >&2; echo "not ok 1 - unexpected apply"; exit 0 ;;
        esac
        ;;
    "TALPHA|load-end|ok=1")
        test "$active" -eq 1 && test "$applies" -eq "$wanted" &&
            test "$seen" -eq "$groups" || {
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
test "$active" -eq 0 && test "$completed" -eq 20 || {
    echo "not ok 1 - unexpected load count"
    exit 0
}
set -x
echo "ok 1 - alpha16 rgb colorsync "
exit 0
