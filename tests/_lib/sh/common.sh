#!/bin/sh
# Common helpers for converter TAP tests executed with POSIX sh.

# Enable strict mode without verbose tracing for cleaner caller logs.
set -ux

# Disable shell tracing temporarily while running a command. This keeps
# stderr clean for assertions that expect no diagnostics while preserving
# verbose logging before and after the call.
run_quiet() {
    status_quiet=0

    set +xv
    "$@" || status_quiet=$?
    set -xv

    return ${status_quiet}
}

# Detect converter enablement using build metadata rather than leftover
# binaries. Stale executables from a previous configuration can linger
# even when converters are disabled, so we rely on the current build
# configuration files to decide whether each tool should be exercised.
#
# The source and build roots can differ between Autotools and Meson
# (particularly during Meson dist checks), so prefer Meson-provided
# locations when available to read the right config headers.

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/../../.." && pwd)
. "${script_dir}/../../lib/sh/common/tap.sh"

if [ -n "${MESON_SOURCE_ROOT:-}" ]; then
    top_srcdir=${TOP_SRCDIR:-${MESON_SOURCE_ROOT}}
else
    top_srcdir=${TOP_SRCDIR:-${parent_dir}}
fi

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}}
fi

images_dir="${top_srcdir}/images"

if [ -z "${IMG2SIXEL_PATH:-}" ]; then
    IMG2SIXEL_PATH="${top_builddir}/converters/img2sixel${SIXEL_BIN_EXT-}"
fi

if [ -z "${SIXEL2PNG_PATH:-}" ]; then
    SIXEL2PNG_PATH="${top_builddir}/converters/sixel2png${SIXEL_BIN_EXT-}"
fi

if [ -z "${LSQA_PATH:-}" ]; then
    LSQA_PATH="${top_builddir}/assessment/lsqa${SIXEL_BIN_EXT-}"
fi

init_config_macro_cache() {
    config_header="${top_builddir}/config.h"

    if [ "${SIXEL_TEST_CONFIG_CACHE_READY:-0}" -eq 1 ]; then
        return 0
    fi

    SIXEL_TEST_CONFIG_CACHE_READY=1
    SIXEL_TEST_DEFINED_MACROS=""

    if [ -f "${config_header}" ]; then
        set -f
        while IFS= read -r line; do
            set -- ${line}
            if [ "${1-}" = "#define" ] && [ "${3-}" = "1" ]; then
                case "$2" in
                HAVE_*)
                    SIXEL_TEST_DEFINED_MACROS="${SIXEL_TEST_DEFINED_MACROS} $2"
                    ;;
                esac
            fi
        done < "${config_header}"
        set +f
    fi

    SIXEL_TEST_DEFINED_MACROS=${SIXEL_TEST_DEFINED_MACROS# }
    export SIXEL_TEST_CONFIG_CACHE_READY
    export SIXEL_TEST_DEFINED_MACROS
    return 0
}

config_macro_defined() {
    macro_name=$1
    macro_value=""

    # Prefer environment-provided feature toggles to avoid file access when
    # the test harness exports config.h-derived values. This keeps direct
    # invocations working because the fallback reads config.h only once.
    eval "macro_value=\${${macro_name}-}"
    if [ -n "${macro_value}" ]; then
        if [ "${macro_value}" -eq 1 ] 2>/dev/null; then
            return 0
        fi
        return 1
    fi

    init_config_macro_cache

    case " ${SIXEL_TEST_DEFINED_MACROS} " in
    *" ${macro_name} "*)
        return 0
        ;;
    esac

    return 1
}

converter_macro_name() {
    case $1 in
    IMG2SIXEL)
        printf 'HAVE_IMG2SIXEL'
        ;;
    SIXEL2PNG)
        printf 'HAVE_SIXEL2PNG'
        ;;
    *)
        printf 'HAVE_%s' "$1"
        ;;
    esac
}

converter_enabled() {
    option_key=$1
    macro_name=$(converter_macro_name "${option_key}")

    if config_macro_defined "${macro_name}"; then
        return 0
    fi

    return 1
}

# Inspect the cached config macros for a given feature. The cache is prepared
# once at startup to reduce repeated file access.
feature_defined_in_config() {
    config_macro_defined "$1"
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

# Confirm that an optional library feature is enabled for the build. The
# autotools path checks the generated config.h macro, and the Meson path
# relies on the corresponding build option. Tests depending on the feature
# use this guard to avoid running under incompatible configurations.
ensure_feature_available() {
    macro_name=$1
    meson_option=$2
    description=$3

    if feature_defined_in_config "${macro_name}"; then
        return 0
    fi

    if [ -n "${meson_option}" ]; then
        :
    fi

    skip_all "${description} is disabled in this build"
}

# Confirm that at least one network backend is enabled. Network tests rely on
# either libcurl or WinHTTP to perform HTTP operations, so allow either backend
# to satisfy the requirement before running the suite.
ensure_network_backend_available() {
    if feature_defined_in_config "HAVE_LIBCURL"; then
        return 0
    fi

    if feature_defined_in_config "HAVE_WINHTTP"; then
        return 0
    fi

    skip_all "libcurl or WinHTTP support is disabled in this build"
}

runtime_exec() {
    if [ -n "${SIXEL_RUNTIME-}" ]; then
        "${SIXEL_RUNTIME-}" "$@"
    else
        "$@"
    fi
}

run_img2sixel() {
    runtime_exec "${IMG2SIXEL_PATH}" "$@"
}

run_sixel2png() {
    runtime_exec "${SIXEL2PNG_PATH}" "$@"
}

run_lsqa() {
    runtime_exec "${LSQA_PATH}" "$@"
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
