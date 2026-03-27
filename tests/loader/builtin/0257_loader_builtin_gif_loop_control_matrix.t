#!/bin/sh
# TAP test: GIF loop_count (NETSCAPE 0/1/2) x loop_control matrix is stable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..9\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_loop0="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop0.gif"
input_loop1="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop1.gif"
input_loop2="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop2.gif"

expected_once="0:0
0:1"
expected_twice="0:0
0:1
1:0
1:1"

extract_sequence() {
    printf '%s\n' "$1" | awk '
        /callback frame_no=/ && /handoff=/ {
            frame = $0
            loop = $0
            sub(/^.*frame_no=/, "", frame)
            sub(/ .*/, "", frame)
            sub(/^.*loop_no=/, "", loop)
            sub(/ .*/, "", loop)
            printf "%s:%s\n", loop, frame
        }'
}

run_finite_case() {
    test_no="$1"
    label="$2"
    loop_mode="$3"
    input="$4"
    expected="$5"

    trace_log=$(
        set +xv
        run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff \
                      -Lbuiltin! "-l${loop_mode}" -g \
                      "${input}" -o /dev/null 2>&1
    ) || {
        echo "not ok" "${test_no}" - "${label} trace run failed"
        return
    }

    actual_sequence="$(extract_sequence "${trace_log}")"
    test "${actual_sequence}" = "${expected}" || {
        echo "not ok" "${test_no}" - "${label} sequence mismatch"
        return
    }

    echo "ok" "${test_no}" - "${label}"
}

run_hang_case() {
    test_no="$1"
    label="$2"
    loop_mode="$3"
    input="$4"

    run_img2sixel -Lbuiltin! "-l${loop_mode}" -g \
                  "${input}" -o /dev/null >/dev/null 2>&1 &
    pid=$!

    wait_limit=3
    while test "${wait_limit}" -gt 0; do
        kill -0 "${pid}" 2>/dev/null || {
            break
        }
        sleep 1
        wait_limit=$((wait_limit - 1))
    done

    kill -0 "${pid}" 2>/dev/null || {
        wait "${pid}" || rc=$?
        echo "not ok" "${test_no}" - "${label} unexpectedly finished"
        return
    }

    kill "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
    echo "ok" "${test_no}" - "${label} stays active as expected"
}

run_finite_case 1 "loop disable ignores NETSCAPE loop0" disable "${input_loop0}" "${expected_once}"
run_finite_case 2 "loop disable ignores NETSCAPE loop1" disable "${input_loop1}" "${expected_once}"
run_finite_case 3 "loop disable ignores NETSCAPE loop2" disable "${input_loop2}" "${expected_once}"
run_finite_case 4 "loop auto respects NETSCAPE loop1" auto "${input_loop1}" "${expected_once}"
run_finite_case 5 "loop auto respects NETSCAPE loop2" auto "${input_loop2}" "${expected_twice}"
run_hang_case 6 "loop auto treats NETSCAPE loop0 as unbounded" auto "${input_loop0}"
run_hang_case 7 "loop force keeps NETSCAPE loop0 unbounded" force "${input_loop0}"
run_hang_case 8 "loop force ignores NETSCAPE loop1" force "${input_loop1}"
run_hang_case 9 "loop force ignores NETSCAPE loop2" force "${input_loop2}"

exit 0
