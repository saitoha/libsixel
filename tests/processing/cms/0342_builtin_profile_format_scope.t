#!/bin/sh
# Policy: docs/loader/color-management.md
# known-profile rejection trace and excluded source/destination controls
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0342_builtin_profile_format_scope 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required CMS or loader unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - known-profile rejection trace and excluded source/destination controls"
    exit 0
}
# Inspect one call at a time using shell expansion only. The boundaries
# keep reasons on the owning call and reject duplicates or stray reasons.
# Avoid repeating the whole remaining transcript in per-iteration xtrace.
# The original trace is retained and printed if any assertion fails.
set +x
remaining=$trace
known=0
positive=0
outside=0
native_controls=0
ok=1
while test "${remaining#*"PFTRACE|kind="}" != "$remaining"; do
    prefix=${remaining%%PFTRACE|kind=*}
    test "${prefix#*"reason=source-profile-format"}" = "$prefix" || ok=0
    test "${prefix#*"reason=native-source-channels"}" = "$prefix" || ok=0
    remaining=${remaining#*PFTRACE|kind=}
    block=${remaining%%PFTRACE|kind=end*}
    test "$block" != "$remaining" || {
        ok=0
        break
    }
    remaining=${remaining#*PFTRACE|kind=end}
    test "${block#*PFTRACE}" = "$block" || ok=0
    marker=${block%%'
'*}
    kind=${marker%%|*}
    test "$kind" = known || test "$kind" = positive ||
        test "$kind" = outside || ok=0
    field=${marker#*|engine=}
    engine=${field%%|*}
    test "$engine" != 2 || engine=builtin
    test "$engine" != 4 || engine=colorsync
    field=${marker#*|src=}
    src=${field%%|*}
    field=${marker#*|expected=}
    expected=${field%%|*}
    rest=$block
    test "$kind" != known || {
        known=$((known + 1))
        field=${marker#*|profile=}
        profile=${field%%|*}
        test "${marker##*|created=}" = 1 || ok=0
        wanted="LSXCMS2|event=reject|engine=$engine|reason=source-profile-format|src=$src|dst=4|profile=$profile|expected=$expected
"
        rest=${rest#*"$wanted"}
        test "$rest" != "$block" || ok=0
        # Remove the single expected reason without hiding earlier reasons.
        prefix=${block%%"$wanted"*}
        rest=$prefix$rest
        wanted="LSXCMS2|event=apply|engine=$engine|path="
        apply=${block#*"$wanted"}
        test "$apply" != "$block" || ok=0
        apply=${apply#*|src=}
        wanted="$src|dst=4|ok=0
"
        test "${apply#"$wanted"}" != "$apply" || ok=0
        prefix=${block%%LSXCMS2|event=apply*}
        apply=${apply#"$wanted"}
        apply=$prefix$apply
        test "${apply#*"LSXCMS2|event=apply"}" = "$apply" || ok=0
    }
    test "$kind" != positive || positive=$((positive + 1))
    test "$kind" != outside || outside=$((outside + 1))
    test "$kind" != outside ||
        test "${marker#*"|native=3|"}" = "$marker" || {
            native_controls=$((native_controls + 1))
            wanted="LSXCMS2|event=reject|engine=colorsync|reason=native-source-channels|src=$src|dst=4|native=3|expected=$expected
"
            rest=${rest#*"$wanted"}
            test "$rest" != "$block" || ok=0
            prefix=${block%%"$wanted"*}
            rest=$prefix$rest
    }
    test "${rest#*"reason=source-profile-format"}" = "$rest" || ok=0
    test "${rest#*"reason=native-source-channels"}" = "$rest" || ok=0
done
set -x
test "${remaining#*"reason=source-profile-format"}" = "$remaining" || ok=0
test "${remaining#*"reason=native-source-channels"}" = "$remaining" || ok=0
test "$known" -eq 376 || ok=0
test "$positive" -eq 18 || ok=0
test "$outside" -eq 5 || ok=0
test "$native_controls" -eq 0 || ok=0
test "$ok" -eq 1 || {
    echo "$trace" >&2
    echo "not ok 1 - scoped profile-domain trace ownership"
    exit 0
}
echo "ok 1 - 0342_builtin_profile_format_scope known-profile rejection trace and excluded source/destination controls"
exit 0
