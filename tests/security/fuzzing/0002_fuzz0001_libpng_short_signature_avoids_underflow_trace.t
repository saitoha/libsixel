#!/bin/sh
# TAP test for fuzz0001: short PNG signature does not trigger APNG underflow trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0001/libpng_short_signature.bin"
test -d "${ARTIFACT_LOCAL_DIR-/tmp}" || mkdir -p "${ARTIFACT_LOCAL_DIR-/tmp}"
trace_log="${ARTIFACT_LOCAL_DIR-/tmp}/fuzz0001_libpng_short_signature.trace"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=apng_decode -Llibpng! "${fuzz_input}" -o /dev/null \
    >/dev/null 2>"${trace_log}"
command_status=$?
set -e

test "${command_status}" -ge 1 || {
    echo "not ok 1 - fuzz0001 did not return mapped error status"
    exit 0
}

test "${command_status}" -le 3 || {
    echo "not ok 1 - fuzz0001 did not return mapped error status"
    exit 0
}

trace_text=
while IFS= read -r trace_line || test -n "${trace_line}"; do
    trace_text="${trace_text}${trace_line}"
done < "${trace_log}"
case "${trace_text}" in
    *remain=1844674407370955*)
        echo "not ok 1 - fuzz0001 still triggers APNG size underflow"
        exit 0
        ;;
esac

echo "ok 1 - fuzz0001 avoids APNG size underflow trace"
exit 0
