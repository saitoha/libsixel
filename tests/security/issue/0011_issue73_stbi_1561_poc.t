#!/bin/sh
# TAP test for issue #73 stbi_1561_poc.bin regression.
# Ensure the converter rejects the input without crashing, even under ASan.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

check_exit() {
    set +e
    run_img2sixel "$@"
    rc=$?
    set -e

    # Accept success or mapped error exits (1/2/3) without crashing.
    [ "${rc}" -le 3 ]
}

issue73="${TOP_SRCDIR}/tests/security/issue/data/libsixel-libsixel/73/stbi_1561_poc.bin"

printf '1..1\n'
set -v

# Use the minimal invocation to exercise the decoder and ensure the
# reported PoC is rejected safely.
check_exit "${issue73}" -o /dev/null || {
    fail 1 "issue #73 PoC handling failed"
    exit 0
}

pass 1 "issue #73 PoC rejected safely"

exit 0
