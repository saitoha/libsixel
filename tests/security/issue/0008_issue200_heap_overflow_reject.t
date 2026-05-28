#!/bin/sh
# TAP test for issue #200 regression using the reported CLI flags.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v



issue200="${TOP_SRCDIR}/tests/data/security/issue/data/200/POC_img2sixel_heap_buffer_overflow"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --7bit-mode -8 --invert --palette-type=auto --verbose         "${issue200}" -o /dev/null
command_status=$?
set -e

# Accept success or mapped error exits without crashing.
test "${command_status}" -le "${SIXEL_TEST_MAX_MAPPED_ERROR_STATUS-3}" || {
    echo "not ok" 1 - "reported heap overflow path failed"
    exit 0
}

echo "ok" 1 - "reported heap overflow path rejected safely"

exit 0
