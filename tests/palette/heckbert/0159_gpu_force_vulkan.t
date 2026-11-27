#!/bin/sh
# Force sends a supported Vulkan histogram to the GPU solver without the
# headroom choice.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP requires img2sixel'
    exit 0
}
echo '1..1'
set -v
trace=$(set +xv; SIXEL_VULKAN_DEBUG=1 ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Gforce -Qheckbert -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1) || {
    printf '\n%s\n' 'not ok 1 - forced Heckbert encoding failed'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *'vulkan compute queue family'*) true ;;
    *)
        printf '\n%s\n' 'ok 1 # SKIP requires a Vulkan device'
        exit 0
        ;;
esac
case "$trace" in
    *LSXHECKPROFILE*) printf '\n%s\n' 'ok 1 - force solves on the GPU' ;;
    *)
        printf '\n%s\n' 'not ok 1 - force did not reach the GPU solver'
        printf '# %s\n' "$trace"
        ;;
esac
exit 0
