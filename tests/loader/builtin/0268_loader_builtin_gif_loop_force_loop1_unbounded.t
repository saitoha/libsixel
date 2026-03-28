#!/bin/sh
# TAP test: loop=force ignores NETSCAPE loop1 and stays unbounded.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v

input_loop1="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop1.gif"
timeout_helper=
timeout_runtime=

command -v timeout >/dev/null 2>&1 && timeout_helper=timeout || true
test -n "${timeout_helper}" || {
    command -v gtimeout >/dev/null 2>&1 && timeout_helper=gtimeout || true
}
test -n "${timeout_helper}" || {
    test -x "${TOP_BUILDDIR}/tools/lso-timeout${EXEEXT-}" &&
        timeout_helper="${TOP_BUILDDIR}/tools/lso-timeout${EXEEXT-}" || true
}
test -n "${timeout_helper}" || {
    printf "1..0 # SKIP timeout helper unavailable\n"
    exit 0
}
test "${timeout_helper}" = timeout || {
    test "${timeout_helper}" = gtimeout || {
        test -n "${SIXEL_RUNTIME-}" && timeout_runtime="${SIXEL_RUNTIME-}" || true
    }
}

rc=0
# shellcheck disable=SC2086
${timeout_runtime-} "${timeout_helper}" -k 1s 3s \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -lforce -g "${input_loop1}" \
    -o /dev/null >/dev/null 2>&1 || rc=$?

test "${rc}" -eq 124 || {
    echo "not ok" 1 - "loop force ignores NETSCAPE loop1 watchdog mismatch"
    exit 0
}

echo "ok" 1 - "loop force ignores NETSCAPE loop1"

exit 0
