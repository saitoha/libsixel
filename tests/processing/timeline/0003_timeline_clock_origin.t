#!/bin/sh
# Verify timeline sessions share the writer clock origin.

set -eux

test "${RUNTIME_ENV_IS_OPENVMS-0}" = "1" && {
    printf "1..0 # SKIP OpenVMS/GNV clock origin trace timing is unstable\n"
    exit 0
}

echo "1..1"
set -v

log_path="${ARTIFACT_ROOT:-.}/timeline_clock_origin.jsonl"
line=
line_count=0
first_line=
second_line=
second_ts_tail=
second_ts=

MSYS_NO_PATHCONV=1 \
MSYS2_ARG_CONV_EXCL='*' \
SIXEL_LOG_PATH="${log_path}" \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0003_timeline_clock_origin" || {
    status=$?
    echo "# test_runner exit status: ${status}"
    echo "# timeline JSONL path: ${log_path}"
    test -f "${log_path}" || {
        echo "# timeline JSONL is missing from TAP shell"
        echo "not ok 1 - 0003_timeline_clock_origin"
        exit 0
    }
    IFS= read -r first_line < "${log_path}" || first_line=
    echo "# timeline JSONL first line: ${first_line}"
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

test -f "${log_path}" || {
    echo "# timeline JSONL is missing from TAP shell"
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

while IFS= read -r line; do
    line_count=$((line_count + 1))
    test "${line_count}" -ne 1 || first_line=${line}
    test "${line_count}" -ne 2 || second_line=${line}
done < "${log_path}"

test "${line_count}" -ge 2 || {
    echo "# timeline JSONL has fewer than two clock records"
    echo "# timeline JSONL first line: ${first_line}"
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

test "${first_line#*\"event\":\"first\"}" != "${first_line}" || {
    echo "# timeline first clock event is missing"
    echo "# timeline JSONL first line: ${first_line}"
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

test "${second_line#*\"event\":\"second\"}" != "${second_line}" || {
    echo "# timeline second clock event is missing"
    echo "# timeline JSONL second line: ${second_line}"
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

second_ts_tail=${second_line#*\"ts\":}
second_ts=${second_ts_tail%%,*}
test -n "${second_ts}" || {
    echo "# timeline second clock timestamp is missing"
    echo "# timeline JSONL second line: ${second_line}"
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

test "${second_ts#0.00}" = "${second_ts}" || {
    echo "# timeline clock origin regressed: second ts=${second_ts}"
    echo "# timeline JSONL first line: ${first_line}"
    echo "# timeline JSONL second line: ${second_line}"
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

echo "ok 1 - 0003_timeline_clock_origin"
exit 0
