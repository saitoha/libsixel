#!/bin/sh
# Exercise concurrent encoder/decoder timeline sessions sharing one writer.

set -eux

echo "1..1"
set -v

log_path="${ARTIFACT_ROOT:-.}/timeline_parallel_encode_decode.jsonl"

MSYS_NO_PATHCONV=1 \
MSYS2_ARG_CONV_EXCL='*' \
SIXEL_LOG_PATH="${log_path}" \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0002_timeline_parallel_encode_decode" || {
    echo "not ok 1 - 0002_timeline_parallel_encode_decode"
    exit 0
}

# Pass the JSONL path through the environment so Git for Windows does not
# reinterpret a native D:/... argv path before MSVC test_runner sees it.
MSYS_NO_PATHCONV=1 \
MSYS2_ARG_CONV_EXCL='*' \
SIXEL_TEST_TIMELINE_JSONL="${log_path}" \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0002_timeline_parallel_encode_decode_verify" || {
    status=$?
    echo "# verifier exit status: ${status}"
    echo "# verifier JSONL path: ${log_path}"
    test -f "${log_path}" || {
        echo "# verifier JSONL is missing from TAP shell"
        echo "not ok 1 - 0002_timeline_parallel_encode_decode_verify"
        exit 0
    }
    IFS= read -r first_line < "${log_path}" || first_line=
    echo "# verifier JSONL first line: ${first_line}"
    echo "not ok 1 - 0002_timeline_parallel_encode_decode_verify"
    exit 0
}

echo "ok 1 - 0002_timeline_parallel_encode_decode"
exit 0
