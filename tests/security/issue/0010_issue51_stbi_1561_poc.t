#!/bin/sh
# TAP test for issue #51 stbi_1561_poc.bin regression.
# Ensure the converter rejects the input without crashing, even under ASan.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


issue51="${TOP_SRCDIR}/tests/data/security/issue/data/libsixel-libsixel/51/stbi_1561_poc.bin"

printf '1..1\n'
set -v

# Use the minimal invocation to exercise the decoder and ensure the
# reported PoC is rejected safely.
set +e
run_img2sixel "${issue51}" -o /dev/null
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    fail 1 "issue #51 PoC handling failed"
    exit 0
}

pass 1 "issue #51 PoC rejected safely"

exit 0
