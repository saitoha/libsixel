#!/bin/sh
# TAP test for fuzz0001: short PNG signature does not trigger APNG underflow trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0001/libpng_short_signature.bin"
trace_log="${ARTIFACT_LOCAL_DIR-/tmp}/fuzz0001_libpng_short_signature.trace"
mkdir -p "$(dirname "${trace_log}")"

set +e
run_img2sixel --env SIXEL_TRACE_TOPIC=apng_decode -Llibpng! "${fuzz_input}" -o /dev/null \
    >/dev/null 2>"${trace_log}"
command_status=$?
set -e

if [ "${command_status}" -lt 1 ] || [ "${command_status}" -gt 3 ]; then
    echo "not ok 1 - fuzz0001 did not return mapped error status"
    exit 0
fi

if [ ! -s "${trace_log}" ] || ! grep -q 'remain=1844674407370955' "${trace_log}"; then
    echo "ok 1 - fuzz0001 avoids APNG size underflow trace"
else
    echo "not ok 1 - fuzz0001 still triggers APNG size underflow"
fi

exit 0
