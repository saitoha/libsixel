#!/bin/sh
# Exercise concurrent encoder/decoder timeline sessions sharing one writer.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

log_path="${ARTIFACT_ROOT:-.}/timeline_parallel_encode_decode.jsonl"
line=
line_count=0
first_line=
first_session=
saw_other_session=0
session_tail=
session_id=

# Keep the test focused on concurrent timeline sessions.  Nested encoder and
# decoder worker pools are not part of this contract and can overwhelm slow
# pthread runtimes.
MSYS_NO_PATHCONV=1 \
MSYS2_ARG_CONV_EXCL='*' \
SIXEL_LOG_PATH="${log_path}" \
SIXEL_THREADS=1 \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0002_timeline_parallel_encode_decode" || {
    status=$?
    echo "# test_runner exit status: ${status}"
    echo "# timeline JSONL path: ${log_path}"
    test -f "${log_path}" || {
        echo "# timeline JSONL is missing from TAP shell"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    IFS= read -r first_line < "${log_path}" || first_line=
    echo "# timeline JSONL first line: ${first_line}"
    echo "not ok 1 - 0002_timeline_parallel_encode_decode"
    exit 0
}

test -f "${log_path}" || {
    echo "# timeline JSONL is missing from TAP shell"
    echo "not ok 1 - 0002_timeline_parallel_encode_decode"
    exit 0
}

while IFS= read -r line; do
    line_count=$((line_count + 1))
    test "${line_count}" -ne 1 || first_line=${line}
    test "${line#*\"ts\":}" != "${line}" || {
        echo "# timeline JSONL line is missing ts: ${line}"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    test "${line#*\"session_id\":}" != "${line}" || {
        echo "# timeline JSONL line is missing session_id: ${line}"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    test "${line#*\"thread\":}" != "${line}" || {
        echo "# timeline JSONL line is missing thread: ${line}"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    test "${line#*\"worker\":}" != "${line}" || {
        echo "# timeline JSONL line is missing worker: ${line}"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    test "${line#*\"role\":}" != "${line}" || {
        echo "# timeline JSONL line is missing role: ${line}"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    test "${line#*\"event\":}" != "${line}" || {
        echo "# timeline JSONL line is missing event: ${line}"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    test "${line#*\"job\":}" != "${line}" || {
        echo "# timeline JSONL line is missing job: ${line}"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    session_tail=${line#*\"session_id\":}
    session_id=${session_tail%%,*}
    test -n "${session_id}" || {
        echo "# timeline JSONL line has invalid session_id: ${line}"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode"
        exit 0
    }
    test -n "${first_session}" || first_session=${session_id}
    test "${session_id}" = "${first_session}" || saw_other_session=1
done < "${log_path}"

test "${line_count}" -gt 0 || {
    echo "# timeline JSONL is empty: ${log_path}"
    echo "not ok 1 - 0002_timeline_parallel_encode_decode"
    exit 0
}

test "${saw_other_session}" = 1 || {
    echo "# timeline JSONL has fewer than two sessions"
    echo "# timeline JSONL first line: ${first_line}"
    echo "not ok 1 - 0002_timeline_parallel_encode_decode"
    exit 0
}

echo "ok 1 - 0002_timeline_parallel_encode_decode"
exit 0
