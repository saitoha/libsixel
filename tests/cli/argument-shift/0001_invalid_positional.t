#!/bin/sh
# TAP test verifying img2sixel rejects invalid positional inputs without
# emitting stray output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

# Skip temporarily on Windows environments while addressing
# intermittent failures specific to that platform.
os_name="${RUNTIME_ENV_BUILD_OS-unknown}"
is_windows_env=0
test "${os_name}" != "${os_name#*mingw*}" && is_windows_env=1
test "${os_name}" != "${os_name#*msys*}" && is_windows_env=1
test "${os_name}" != "${os_name#*cygwin*}" && is_windows_env=1
test "${is_windows_env}" = 0 || {
    printf "1..0 # SKIP temporarily disabled on Windows due to instability\n"
    exit 0
}


echo "1..1"
set -v

missing_path="${TOP_SRCDIR}/tests/data/inputs/does-not-exist-invalid-positional.ppm"
missing_stdout=''

missing_stdout=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "${missing_path}" 2>/dev/null) && {
    echo "not ok" 1 - "img2sixel accepted missing input"
    exit 0
}
: "${missing_stdout}"

test -z "${missing_stdout}" || {
    echo "not ok" 1 - "img2sixel produced output for missing input"
    exit 0
}

echo "ok" 1 - "missing input rejected without output"
exit 0
