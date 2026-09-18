#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management.md
# Decoded YCbCr RGB8: builtin.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
unset SIXEL_ALPHA_POLICY
set +x
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0467_ycbcr_builtin "${TOP_SRCDIR}/tests/data/color-numeric" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - ycbcr builtin"
    exit 0
}
active=0
applies=0
completed=0
engine=none
source=0
wanted=0
while IFS= read -r line; do
    case "$line" in
    "TYC|begin|"*)
        test "$active" -eq 0 || {
            echo "not ok 1 - overlapping loader phases"
            exit 0
        }
        active=1
        applies=0
        wanted=${line##*=}
        engine=${line#*engine=}
        engine=${engine%%|*}
        source=${line#*src=}
        source=${source%%|*}
        ;;
    *"LSXCMS2|event=apply|"*)
        # Independent reference conversions run outside the loader phase.
        test "$active" -ne 0 || continue
        case "${line}" in *"|engine=$engine|"*) true ;; *) false ;; esac &&
            case "${line}" in *"|src=$source|dst=4|"*) true ;; *) false ;; esac &&
            test "${line##*|ok=}" -eq 1 || {
            echo "$trace" >&2
            echo "not ok 1 - wrong actual CMS backend, format or result"
            exit 0
        }
        applies=$((applies + 1))
        ;;
    "TYC|end|ok=1")
        test "$active" -eq 1 && test "$applies" -eq "$wanted" || {
            echo "$trace" >&2
            echo "not ok 1 - unexpected actual CMS apply count"
            exit 0
        }
        active=0
        completed=$((completed + 1))
        ;;
    esac
done <<EOF
$trace
EOF
test "$active" -eq 0 && test "$completed" -eq 1 || {
    echo "$trace" >&2
    echo "not ok 1 - incomplete loader phases"
    exit 0
}
echo "ok 1 - ycbcr builtin"
exit 0
