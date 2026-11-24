#!/bin/sh
# Verify the CLI fixture replaces a live, unrelated terminal on stdin.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 -a "${HAVE_LIBPNG-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel or libpng unavailable'
    exit 0
}
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0039_cli_color_auto \
    parent-stdin "${IMG2SIXEL_PATH}" \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-16-rgb.png" || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP scripted POSIX terminal I/O is unavailable'
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - CLI fixture is isolated from its parent terminal'
    exit 0
}
printf '\n%s\n' 'ok 1 - CLI fixture is isolated from its parent terminal'
exit 0
