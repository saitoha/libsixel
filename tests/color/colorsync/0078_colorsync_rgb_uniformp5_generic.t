#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify ColorSync RGB uniform.5 generic against original stored ICC values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0053_colorsync_rgb_power 30 generic colorsync 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - ColorSync RGB uniform.5 generic"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|src=4|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - original RGB float did not use ColorSync"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=builtin|"*) false ;; *) true ;; esac || {
    echo "not ok 1 - builtin substituted for ColorSync"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=lcms2|"*) false ;; *) true ;; esac || {
    echo "not ok 1 - Little CMS substituted for ColorSync"
    exit 0
}
echo "ok 1 - ColorSync RGB uniform.5 generic"
exit 0
