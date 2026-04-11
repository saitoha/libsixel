#!/bin/sh
# TAP test confirming builtin HDR cms=auto avoids header-profile unavailable trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

input_hdr="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal.hdr"
trace_status=0
trace_log=''

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms_engine=auto! \
    "${input_hdr}" -o /dev/null 2>&1) || trace_status=$?

test "${trace_status}" -eq 0 || {
    echo "not ok" 1 - "builtin HDR cms=auto trace run failed"
    exit 0
}
test "${trace_log#*header-derived source profile is unavailable on this CMS backend*}" = "${trace_log}" || {
    echo "not ok" 1 - "builtin HDR cms=auto emitted unexpected header-profile unavailable trace"
    exit 0
}

echo "ok" 1 - "builtin HDR cms=auto does not emit header-profile unavailable trace"
exit 0
