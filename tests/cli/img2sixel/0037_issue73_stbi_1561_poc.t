#!/bin/sh
# TAP test for issue #73 stbi_1561_poc.bin regression.
# Ensure the converter rejects the input without crashing, even under ASan.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v


issue73="${TOP_SRCDIR}/tests/data/security/issue/data/libsixel-libsixel/73/stbi_1561_poc.bin"

# Use the minimal invocation to exercise the decoder and ensure the
# reported PoC is rejected safely.
set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${issue73}" -o /dev/null
command_status=$?
set -e

# Accept success or mapped error exits without crashing.
test "${command_status}" -le "${SIXEL_TEST_MAX_MAPPED_ERROR_STATUS-3}" || {
    echo "not ok" 1 - "issue #73 PoC handling failed"
    exit 0
}

echo "ok" 1 - "issue #73 PoC rejected safely"

exit 0
