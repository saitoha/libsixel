#!/bin/sh
# TAP test for fuzz0005: empty minimized loader-order input does not crash.
# Minimized reproducer from libFuzzer artifact:
# crash-da39a3ee5e6b4b0d3255bfef95601890afd80709 (empty file)
# Reproducer job:
# https://github.com/saitoha/libsixel/actions/runs/23972047719/job/69923134105

set -eux

harness="${TOP_BUILDDIR}/fuzz/fuzz-loader-builtin-struct-loader-order-libfuzzer${SIXEL_BIN_EXT-}"
test -x "${harness}" || {
    printf "1..0 # SKIP loader-order fuzz harness is not built\n"
    exit 0
}


echo "1..1"
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0005/loader_order_empty.bin"

test -f "${fuzz_input}" || {
    echo "not ok" 1 - "fuzz0005 input is missing"
    exit 0
}

test ! -s "${fuzz_input}" || {
    echo "not ok" 1 - "fuzz0005 input is not minimized"
    exit 0
}

rc=0
${SIXEL_RUNTIME-} "${harness}" -runs=1 "${fuzz_input}" >/dev/null 2>&1 || rc=$?

test "${rc}" -eq 0 || {
    echo "not ok" 1 - "fuzz0005 empty input crashed loader-order harness"
    exit 0
}

echo "ok" 1 - "fuzz0005 empty input is handled without crash"


exit 0
