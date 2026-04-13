#!/bin/sh
# TAP test for fuzz0007: minimized CoreGraphics indexed OOM input is rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0007/coregraphics_indexed_provider_copy_oom_min.bmp"

test -f "${fuzz_input}" || {
    echo "not ok" 1 - "fuzz0007 input is missing"
    exit 0
}

size=$(wc -c < "${fuzz_input}")
test "${size}" -eq 33 || {
    echo "not ok" 1 - "fuzz0007 input is not minimized"
    exit 0
}

rc=0
set +e
err="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lcoregraphics! "${fuzz_input}" -o /dev/null 2>&1)"
rc=$?
set -e

test "${rc-0}" -ge 1 || {
    echo "not ok" 1 - "fuzz0007 did not return mapped error status"
    exit 0
}

test "${rc-0}" -le 3 || {
    echo "not ok" 1 - "fuzz0007 did not return mapped error status"
    exit 0
}

test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "fuzz0007 did not execute img2sixel"
    exit 0
}

test "${rc-0}" -ne 134 || {
    echo "not ok" 1 - "fuzz0007 triggered abort"
    exit 0
}

test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "fuzz0007 triggered SIGSEGV"
    exit 0
}

test "${err#*indexed source row payload is too large*}" != "${err}" || {
    echo "not ok" 1 - "fuzz0007 did not hit indexed row-size guard"
    exit 0
}

echo "ok" 1 - "fuzz0007 minimized coregraphics indexed input is rejected safely"


exit 0
