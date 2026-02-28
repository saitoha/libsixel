#!/bin/sh
# TAP test for issue #73 stbi_1561_poc.bin regression.
# Ensure the converter rejects the input without crashing, even under ASan.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

issue73="${TOP_SRCDIR}/tests/data/security/issue/data/libsixel-libsixel/73/stbi_1561_poc.bin"

printf '1..1\n'
set -v

# Use the minimal invocation to exercise the decoder and ensure the
# reported PoC is rejected safely.
set +e
run_img2sixel "${issue73}" -o /dev/null
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    echo "not ok" 1 "issue #73 PoC handling failed"
    exit 0
}

echo "ok" 1 "issue #73 PoC rejected safely"

exit 0
