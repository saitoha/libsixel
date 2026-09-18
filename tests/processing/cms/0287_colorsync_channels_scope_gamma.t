#!/bin/sh
# Policy: docs/loader/color-management.md
# UNKNOWN native values, channel layout, and rejection reason
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0287_colorsync_channels_scope_gamma 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync or required loader unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - UNKNOWN native values, channel layout, and rejection reason"
    exit 0
}
# Match the trace directly. Shortest-prefix removal repeatedly scans long
# prefixes in some shells and makes these presence checks quadratic.
case "$trace" in
    *"LSXCMS1|intent_order=relative|exclusive=1"*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - missing expected contract trace"
        exit 0
    ;;
esac
case "$trace" in
    *"xyz-values-native-units-and-guards-verified"*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - missing expected contract trace"
        exit 0
    ;;
esac
case "$trace" in
    *"LSXCMS2|event=reject|engine=colorsync|reason=native-source-channels|src=1|dst=4|native=3|expected=1"*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - missing expected contract trace"
        exit 0
    ;;
esac
case "$trace" in
    *"LSXCMS2|event=reject|engine=colorsync|reason=native-source-channels|src=2|dst=4|native=4|expected=3"*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - missing expected contract trace"
        exit 0
    ;;
esac
case "$trace" in
    *"LSXCMS2|event=reject|engine=colorsync|reason=native-source-channels|src=3|dst=4|native=4|expected=3"*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - missing expected contract trace"
        exit 0
    ;;
esac
case "$trace" in
    *"LSXCMS2|event=reject|engine=colorsync|reason=native-source-channels|src=5|dst=4|native=3|expected=4"*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - missing expected contract trace"
        exit 0
    ;;
esac
case "$trace" in
    *"LSXCMS2|event=reject|engine=colorsync|reason=native-source-channels|src=6|dst=4|native=3|expected=4"*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - missing expected contract trace"
        exit 0
    ;;
esac
case "$trace" in
    *"LSXCMS2|event=reject|engine=colorsync|reason=native-source-channels|src=8|dst=4|native=4|expected=3"*) ;;
    *)
        echo "$trace" >&2
        echo "not ok 1 - missing expected contract trace"
        exit 0
    ;;
esac
echo "ok 1 - 0287_colorsync_channels_scope_gamma UNKNOWN native values, channel layout, and rejection reason"
exit 0
