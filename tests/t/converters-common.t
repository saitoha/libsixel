#!/bin/sh
# Common helpers for converter TAP tests executed with POSIX sh.

set -u

# Detect converter enablement using build metadata rather than leftover
# binaries. Stale executables from a previous configuration can linger
# even when converters are disabled, so we rely on the current build
# configuration files to decide whether each tool should be exercised.

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/.." && pwd)

top_srcdir=${TOP_SRCDIR:-${parent_dir}}
top_builddir=${TOP_BUILDDIR:-${parent_dir}}

images_dir="${top_srcdir}/images"

if [ -z "${IMG2SIXEL_PATH:-}" ]; then
    IMG2SIXEL_PATH="${top_builddir}/converters/img2sixel"
fi

if [ -z "${SIXEL2PNG_PATH:-}" ]; then
    SIXEL2PNG_PATH="${top_builddir}/converters/sixel2png"
fi

check_autotools_option() {
    program_name=$1

    makefile_path="${top_builddir}/converters/Makefile"
    if [ ! -f "${makefile_path}" ]; then
        return 1
    fi

    if grep -Eq "[[:space:]]${program_name}\\$\\(EXEEXT\\)" \
            "${makefile_path}"; then
        return 0
    fi

    return 1
}

check_meson_option() {
    option_name=$1

    options_path="${top_builddir}/meson-info/intro-buildoptions.json"
    if [ ! -f "${options_path}" ]; then
        return 1
    fi

    python3 - "$option_name" "$options_path" <<'PY' || exit 1
import json
import sys

option = sys.argv[1]
path = sys.argv[2]

with open(path, 'r', encoding='utf-8') as fh:
    data = json.load(fh)

for entry in data:
    if entry.get('name') == option:
        value = entry.get('value')
        if value in ('enabled', True):
            sys.exit(0)
        break

sys.exit(1)
PY
}

converter_enabled() {
    option_key=$1
    meson_key=$(printf '%s' "${option_key#ENABLE_}" | tr 'A-Z' 'a-z')

    if check_autotools_option "${meson_key}"; then
        return 0
    fi

    if check_meson_option "${meson_key}"; then
        return 0
    fi

    return 1
}

skip_all() {
    echo "1..0 # SKIP $1"
    exit 0
}

ensure_executable() {
    target_path=$1
    description=$2

    if [ -x "${target_path}" ]; then
        return 0
    fi

    if [ -e "${target_path}" ]; then
        skip_all "${description} is not executable"
    fi

    skip_all "${description} is not available (not built)"
}

# Confirm converter availability using build metadata and skip gracefully
# when the tool is disabled or missing. This keeps stale binaries from a
# previous configuration from influencing the decision.
ensure_converter_available() {
    option_key=$1
    target_path=$2
    description=$3

    if ! converter_enabled "${option_key}"; then
        skip_all "${description} is disabled in this build"
    fi

    ensure_executable "${target_path}" "${description}"
}

wine_exec() {
    if [ -n "${WINE:-}" ]; then
        "${WINE}" "$@"
    else
        "$@"
    fi
}

run_img2sixel() {
    wine_exec "${IMG2SIXEL_PATH}" "$@"
}

run_sixel2png() {
    wine_exec "${SIXEL2PNG_PATH}" "$@"
}

make_temp_file() {
    dir_path=$1
    prefix=$2

    if command -v mktemp >/dev/null 2>&1; then
        mktemp "${dir_path}/${prefix}.XXXXXX"
        return 0
    fi

    candidate="${dir_path}/${prefix}.$$"
    : >"${candidate}"
    echo "${candidate}"
    return 0
}

require_file() {
    if [ ! -e "$1" ]; then
        echo "Required file '$1' is missing" >&2
        exit 1
    fi
}

export SIXEL_THREADS=${SIXEL_THREADS:-1}
