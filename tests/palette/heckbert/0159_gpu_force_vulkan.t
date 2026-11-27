#!/bin/sh
# Policy: docs/encoder/palette-construction/quantization/README.md
# Force sends a supported Vulkan histogram to the GPU solver without the
# headroom choice.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP requires img2sixel'
    exit 0
}
test "${HAVE_VULKAN-}" = 1 && test "${HAVE_METAL-}" != 1 || {
    echo '1..0 # SKIP requires the Vulkan Heckbert backend'
    exit 0
}
echo '1..1'
set -v
status=0
probe_status=0
# Discard SIXEL output through stdout so the encoder needs no output file.
trace=$(set +xv; SIXEL_VULKAN_DEBUG=1 ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Gforce -Qheckbert \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1 >/dev/null) ||
    status=$?
# Probe independently of encoding so a failure on an available device fails.
test "$status" -eq 0 ||
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --gpu-lookup-available ordered ||
    probe_status=$?
test "$probe_status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP requires a Vulkan device'
    exit 0
}
test "$probe_status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - Vulkan device probe failed'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - forced Heckbert encoding failed'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *LSXHECKPROFILE*) printf '\n%s\n' 'ok 1 - force solves on the GPU' ;;
    *)
        printf '\n%s\n' 'not ok 1 - force did not reach the GPU solver'
        printf '# %s\n' "$trace"
        ;;
esac
exit 0
