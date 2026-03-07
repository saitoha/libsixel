#!/bin/sh
# TAP test for fuzz0001: short PNG signature must not underflow APNG parser.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
printf '1..2\n'
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0001/libpng_short_signature.bin"
trace_log="${ARTIFACT_LOCAL_DIR-/tmp}/fuzz0001_libpng_short_signature.trace"

set +e
run_img2sixel --env SIXEL_TRACE_TOPIC=apng_decode -Llibpng! "${fuzz_input}" -o /dev/null \
    > /dev/null 2> "${trace_log}"
command_status=$?
set -e

test "${command_status}" -ge 1 -a "${command_status}" -le 3 || {
    echo "not ok" 1 - "fuzz0001 did not return mapped error status"
    exit 0
}

echo "ok" 1 - "fuzz0001 rejected with mapped error status"

test ! -s "${trace_log}" || ! grep -q 'remain=1844674407370955' "${trace_log}" || {
    echo "not ok" 2 - "fuzz0001 still triggers APNG size underflow"
    exit 0
}

echo "ok" 2 - "fuzz0001 avoids APNG size underflow"

exit 0
