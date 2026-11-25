#!/bin/sh
# libpng reports retained RGBA and mask storage.
set -eux
echo '1..1'
set -v

test "${HAVE_LIBPNG-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP libpng is unavailable'
    exit 0
}
export SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical png-storage || {
    printf '\n%s\n' 'not ok 1 - libpng reports retained RGBA and mask storage'
    exit 0
}
printf '\n%s\n' 'ok 1 - libpng reports retained RGBA and mask storage'
exit 0
