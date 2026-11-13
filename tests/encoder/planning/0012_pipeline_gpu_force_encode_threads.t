#!/bin/sh
# TAP test: forced GPU palette apply leaves post-palette workers for encode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

echo "1..1"
set -v

# Probe only failed execution; an encode error on a capable device must fail.
conversion_status=0
probe_status=0
pipeline_log=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_ENCODER_THREADING_POLICY=greedy --env SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=4 \
                  -G force -d none -p 16 --lookup-policy=none \
                  -v -o /dev/null "${probe_image}" 2>&1) || conversion_status=$?
printf '%s' "${pipeline_log}" >&2
test "$conversion_status" -eq 0 ||
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --gpu-lookup-available none || probe_status=$?
test "$probe_status" -ne 77 || {
    echo "ok 1 # SKIP GPU direct lookup is unavailable"
    exit 0
}
test "$probe_status" -eq 0 || {
    echo "not ok 1 - GPU direct lookup probe failed"
    exit 0
}
test "$conversion_status" -eq 0 || {
    echo "not ok" 1 - "gpu force thread split run failed"
    exit 0
}

case "${pipeline_log}" in
    *"threads: dither=0 encode="*) ;;
    *)
        echo "not ok" 1 - "gpu force keeps encode worker budget"
        exit 0
        ;;
esac

echo "ok" 1 - "gpu force keeps encode worker budget"
exit 0
