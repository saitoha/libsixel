#!/bin/sh
# Generate amalgamated sixel.c that stitches together all primary C sources.
# Usage: gen-amalgamation.sh OUTPUT [SOURCE_ROOT] [BUILD_ROOT] [UNITS...]
# SOURCE_ROOT defaults to the repository root relative to this script.
# BUILD_ROOT defaults to SOURCE_ROOT. All comment strings must stay in English.
# UNITS may be provided to override the default source ordering. Each entry is
# treated as relative to SOURCE_ROOT unless an absolute path is specified.

set -eu

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 OUTPUT [SOURCE_ROOT] [BUILD_ROOT] [UNITS...]" >&2
    exit 1
fi

output=$1
shift
script_dir=$(cd "$(dirname "$0")" && pwd)
default_root=$(cd "${script_dir}/.." && pwd)
src_root_arg=${1:-${default_root}}
build_root_arg=${2:-${src_root_arg}}
src_root=$(cd "${src_root_arg}" && pwd)
build_root=$(cd "${build_root_arg}" && pwd)
shift 2 || true

mkdir -p "$(dirname "${output}")"

default_units="
src/compat_stub.c
src/output.c
src/fromsixel.c
src/tosixel.c
src/timer.c
src/assessment.c
src/sleep.c
src/lookup-common.c
src/lookup-8bit.c
src/lookup-float32.c
src/lookup-vpte-8bit.c
src/lookup-vpte-float32.c
src/palette-heckbert.c
src/palette-common-merge.c
src/palette-common-snap.c
src/palette-kmeans.c
src/palette.c
src/dither.c
src/dither-common-pipeline.c
src/dither-positional-8bit.c
src/dither-positional-float32.c
src/dither-fixed-8bit.c
src/dither-fixed-float32.c
src/dither-varcoeff-8bit.c
src/dither-varcoeff-float32.c
src/logger.c
src/colorspace.c
src/frame.c
src/filter.c
src/filter-clip.c
src/filter-colors.c
src/filter-dither.c
src/filter-encode.c
src/filter-factory.c
src/filter-final-merge.c
src/filter-load.c
src/filter-lookup.c
src/filter-palette.c
src/filter-resize.c
src/filter-sample.c
src/filter-vpte.c
src/cpu.c
src/pixelformat.c
src/scale.c
src/chunk.c
src/loader.c
src/loader-gdk-pixbuf2.c
src/loader-gnome-thumbnailer.c
src/loader-coregraphics.c
src/loader-quicklook.c
src/loader-wic.c
src/loader-gd.c
src/loader-libpng.c
src/loader-libjpeg.c
src/loader-builtin.c
src/loader-common.c
src/loader-registry.c
src/frompnm.c
src/fromgif.c
src/clipboard.c
src/encoder.c
src/decoder.c
src/decoder-parallel.c
src/options.c
src/writer.c
src/stb_image_write.c
src/status.c
src/malloc_stub.c
src/allocator.c
src/tty.c
src/threading.c
src/threadpool.c
src/quicklook_thumbnailing.m
src/clipboard_macos.m
src/clipboard_carbon.c
src/probe.c
src/tests.c
tests/gdk-pixbuf-loader/test_0002_corrupt_data.c
tests/gdk-pixbuf-loader/test-gdk-pixbuf-loader.c
tests/gdk-pixbuf-loader/test_0004_context_free.c
tests/gdk-pixbuf-loader/test_0001_incremental_load.c
tests/gdk-pixbuf-loader/test_0003_propagate_error.c
tests/cli/test_0023_cli_token_is_known_option.c
tests/cli/test_0024_cli_option_requires_argument.c
tests/cli/test_0025_cli_guard_missing_argument.c
tests/filter/filter_colors_tests.c
tests/filter/filter_encode_tests.c
tests/filter/filter_vpte_tests.c
tests/filter/filter_dither_tests.c
tests/filter/filter_resize_tests.c
tests/filter/filter_final_merge_tests.c
tests/filter/filter_load_tests.c
tests/filter/filter_tests.c
tests/filter/filter_lookup_tests.c
tests/filter/filter_sample_tests.c
tests/probe/test_probe_parse.c
tests/palette/test_palette_kmeans_init.c
converters/aborttrace.c
converters/cli.c
converters/completion_utils.c
converters/img2sixel.c
converters/malloc_stub.c
converters/sixel2png.c
assessment/lsqa.c
"

project_headers="
sixel.h
sixel_threads_config.h
sixel_atomic.h
allocator.h
logger.h
threading.h
threadpool.h
status.h
cpu.h
pixelformat.h
colorspace.h
palette.h
palette-common-merge.h
palette-common-snap.h
palette-heckbert.h
palette-kmeans.h
dither.h
dither-internal.h
dither-common-pipeline.h
dither-fixed-8bit.h
dither-fixed-float32.h
dither-positional-8bit.h
dither-positional-float32.h
dither-varcoeff-8bit.h
dither-varcoeff-float32.h
lookup-vpte-8bit.h
lookup-vpte-float32.h
lookup-8bit.h
lookup-float32.h
lookup-common.h
filter.h
filter-clip.h
filter-colors.h
filter-dither.h
filter-encode.h
filter-factory.h
filter-final-merge.h
filter-load.h
filter-lookup.h
filter-palette.h
filter-resize.h
filter-sample.h
filter-vpte.h
frame.h
scale.h
options.h
assessment.h
encoder.h
decoder.h
decoder-image.h
decoder-parallel.h
fromgif.h
frompnm.h
output.h
writer.h
chunk.h
tty.h
timer.h
sleep.h
clipboard.h
probe.h
loader.h
loader-builtin.h
loader-common.h
loader-registry.h
loader-libpng.h
loader-libjpeg.h
loader-gd.h
loader-gdk-pixbuf2.h
loader-gnome-thumbnailer.h
loader-coregraphics.h
loader-quicklook.h
loader-wic.h
stb_image.h
stb_image_write.h
malloc_stub.h
stdio_stub.h
rgblookup.h
lso2.h
compat_stub.h
fromgif.h
frompnm.h
aborttrace.h
cli.h
completion_utils.h
completion_embed.h
getopt_stub.h
malloc_stub.h
"

header_units="
include/sixel.h
src/sixel_threads_config.h
src/sixel_atomic.h
src/threading.h
src/threadpool.h
src/allocator.h
src/logger.h
src/status.h
src/cpu.h
src/pixelformat.h
src/colorspace.h
src/palette.h
src/palette-common-merge.h
src/palette-common-snap.h
src/palette-heckbert.h
src/palette-kmeans.h
src/dither.h
src/dither-internal.h
src/dither-common-pipeline.h
src/dither-fixed-8bit.h
src/dither-fixed-float32.h
src/dither-positional-8bit.h
src/dither-positional-float32.h
src/dither-varcoeff-8bit.h
src/dither-varcoeff-float32.h
src/lookup-vpte-8bit.h
src/lookup-vpte-float32.h
src/lookup-8bit.h
src/lookup-float32.h
src/lookup-common.h
src/filter.h
src/filter-clip.h
src/filter-colors.h
src/filter-dither.h
src/filter-encode.h
src/filter-factory.h
src/filter-final-merge.h
src/filter-load.h
src/filter-lookup.h
src/filter-palette.h
src/filter-resize.h
src/filter-sample.h
src/filter-vpte.h
src/frame.h
src/scale.h
src/options.h
src/assessment.h
src/encoder.h
src/decoder.h
src/decoder-image.h
src/decoder-parallel.h
src/fromgif.h
src/frompnm.h
src/output.h
src/writer.h
src/chunk.h
src/tty.h
src/timer.h
src/sleep.h
src/clipboard.h
src/probe.h
src/loader.h
src/loader-builtin.h
src/loader-common.h
src/loader-registry.h
src/loader-libpng.h
src/loader-libjpeg.h
src/loader-gd.h
src/loader-gdk-pixbuf2.h
src/loader-gnome-thumbnailer.h
src/loader-coregraphics.h
src/loader-quicklook.h
src/loader-wic.h
src/malloc_stub.h
src/stdio_stub.h
src/rgblookup.h
src/lso2.h
src/compat_stub.h
converters/aborttrace.h
converters/cli.h
converters/completion_utils.h
converters/completion_embed.h
converters/getopt_stub.h
converters/malloc_stub.h
"

# Persist header names to a temporary file so portable awk can read them
# without depending on vendor-specific split() behavior.
header_temp=$(mktemp "${TMPDIR:-/tmp}/sixel-headers.XXXXXX")
license_temp=$(mktemp "${TMPDIR:-/tmp}/sixel-licenses.XXXXXX")
license_hash_temp=$(mktemp "${TMPDIR:-/tmp}/sixel-license-hash.XXXXXX")
trap 'rm -f "${header_temp}" "${license_temp}" "${license_hash_temp}"' EXIT
printf "%s\n" "${project_headers}" >"${header_temp}"

# Headers are concatenated upfront so that the generated translation unit does
# not rely on project-local includes at compile time.

if [ "$#" -gt 0 ]; then
    user_units="$*"
else
    user_units="${default_units}"
fi

config_path="${build_root}/config.h"

append_unique_unit() {
    candidate=$1
    pattern=$(printf '\n%s\n' "${candidate}")

    case "${user_units}" in
        *"${pattern}"*)
            return
            ;;
        *)
            user_units="${user_units}
${candidate}"
            ;;
    esac
}

resolve_unit() {
    case "$1" in
        /*)
            echo "$1"
            ;;
        *)
            echo "${src_root}/$1"
            ;;
    esac
}

resolve_header_unit() {
    case "$1" in
        /*)
            echo "$1"
            ;;
        *)
            echo "${src_root}/$1"
            ;;
    esac
}

extract_license_block() {
    unit_path=$1

    awk '
BEGIN {
    in_block = 0;
    collected = 0;
}
NR == 1 {
    sub(/^\xef\xbb\xbf/, "");
}
in_block == 0 && collected == 0 {
    if ($0 ~ /^[ \t]*$/) {
        next;
    }
    if ($0 ~ /^[ \t]*\/\*/) {
        in_block = 1;
        collected = 1;
        print $0;
        if ($0 ~ /\*\//) {
            exit;
        }
        next;
    }
    exit;
}
in_block == 1 {
    print $0;
    if ($0 ~ /\*\//) {
        exit;
    }
    next;
}
' "${unit_path}"
}

append_license_block() {
    unit_path=$1

    if [ ! -f "${unit_path}" ]; then
        return
    fi

    block=$(extract_license_block "${unit_path}") || true
    if [ -z "${block}" ]; then
        return
    fi

    hash=$(printf "%s" "${block}" | cksum | awk '{print $1}')
    if grep -q "^${hash}$" "${license_hash_temp}" 2>/dev/null; then
        return
    fi

    printf "%s\n" "${hash}" >>"${license_hash_temp}"
    {
        printf "/* ==== LICENSE from %s ==== */\n" "${unit_path}"
        printf "%s\n\n" "${block}"
    } >>"${license_temp}"
}

collect_license_blocks() {
    printf "%s\n" "${header_units}" | while IFS= read -r unit; do
        [ -z "${unit}" ] && continue

        case "${unit}" in
            src/threadpool.h)
                append_license_block "$(resolve_header_unit "${unit}")"
                ;;
            src/loader-quicklook.h|src/loader-coregraphics.h|src/loader-wic.h)
                append_license_block "$(resolve_header_unit "${unit}")"
                ;;
            include/sixel.h)
                public_header="${build_root}/include/sixel.h"
                if [ -f "${public_header}" ]; then
                    append_license_block "${public_header}"
                else
                    append_license_block "$(resolve_header_unit "${unit}")"
                fi
                ;;
            *)
                append_license_block "$(resolve_header_unit "${unit}")"
                ;;
        esac
    done

    printf "%s\n" "${user_units}" | while IFS= read -r unit; do
        [ -z "${unit}" ] && continue

        append_license_block "$(resolve_unit "${unit}")"
    done
}

# Remove project-local includes so the generated translation unit can compile
# without the original header files present. stb headers are inlined once at
# the first include site so that implementation guards remain effective.
filter_local_includes() {
    unit_path=$1
    header_file=$2
    src_root=$3

    awk -v header_file="${header_file}" -v src_root="${src_root}" '
BEGIN {
    while ((getline line < header_file) > 0) {
        gsub(/^[ \t]+|[ \t]+$/, "", line);
        if (length(line) > 0) {
            header[line] = 1;
        }
    }
    close(header_file);

    inline_once["stb_image.h"] = 1;
    inline_once["stb_image_write.h"] = 1;
}
{
    if ($0 ~ /^[ \t]*#pragma[ \t]+once[ \t]*$/) {
        print "/* #pragma once */";
        next;
    }
    if ($0 ~ /^[ \t]*#[ \t]*include[ \t]*["<][^">]+[">]/) {
        name = $0;
        sub(/^[ \t]*#[ \t]*include[ \t]*["<]/, "", name);
        sub(/[">].*/, "", name);
            if (name in header) {
                if (name in inline_once && !(name in emitted)) {
                    emitted[name] = 1;
                    path = src_root "/src/" name;
                    if ((getline line < path) > 0) {
                        print "#line 1 \"" path "\"";
                        if (line ~ /^[ \t]*#pragma[ \t]+once[ \t]*$/) {
                            print "/* #pragma once */";
                        } else {
                            print line;
                        }
                        while ((getline line < path) > 0) {
                            if (line ~ /^[ \t]*#pragma[ \t]+once[ \t]*$/) {
                                print "/* #pragma once */";
                                continue;
                            }
                            print line;
                        }
                        close(path);
                        print "";
                    }
                }
            next;
        }
    }
    print $0;
}
' "${unit_path}"
}

emit_unit() {
    unit_path=$(resolve_unit "$1")
    guard_expr=$2

    if [ ! -f "${unit_path}" ]; then
        echo "Skip missing unit: ${unit_path}" >&2
        return
    fi

    if [ -n "${guard_expr}" ]; then
        printf "\n#if %s\n" "${guard_expr}" >>"${output}"
    fi

    printf "\n/* ==== %s ==== */\n" "$1" >>"${output}"
    printf "#line 1 \"%s\"\n" "${unit_path}" >>"${output}"
    filter_local_includes "${unit_path}" "${header_temp}" \
        "${src_root}" >>"${output}"
    printf "\n" >>"${output}"

    if [ -n "${guard_expr}" ]; then
        printf "#endif /* %s */\n" "${guard_expr}" >>"${output}"
    fi
}

emit_header_unit() {
    unit_path=$(resolve_unit "$1")
    guard_expr=$2

    if [ ! -f "${unit_path}" ]; then
        echo "Skip missing header: ${unit_path}" >&2
        return
    fi

    if [ -n "${guard_expr}" ]; then
        printf "\n#if %s\n" "${guard_expr}" >>"${output}"
    fi

    printf "\n/* ==== %s ==== */\n" "$1" >>"${output}"
    printf "#line 1 \"%s\"\n" "${unit_path}" >>"${output}"
    filter_local_includes "${unit_path}" "${header_temp}" \
        "${src_root}" >>"${output}"
    printf "\n" >>"${output}"

    if [ -n "${guard_expr}" ]; then
        printf "#endif /* %s */\n" "${guard_expr}" >>"${output}"
    fi
}

emit_all_headers() {
    echo "${header_units}" | while IFS= read -r unit; do
        [ -z "${unit}" ] && continue

        case "${unit}" in
            src/threadpool.h)
                emit_header_unit "${unit}" "SIXEL_ENABLE_THREADS"
                ;;
            converters/completion_embed.h)
                embedded_header="${build_root}/converters/completion_embed.h"
                if [ ! -f "${embedded_header}" ]; then
                    embedded_header="${build_root}/amalgamation/completion_embed.h"
                fi
                if [ -f "${embedded_header}" ]; then
                    emit_header_unit "${embedded_header}" \
                        "defined(BUILD_IMG2SIXEL)"
                fi
                ;;
            converters/cli.h|converters/completion_utils.h|converters/getopt_stub.h|converters/malloc_stub.h|converters/aborttrace.h)
                emit_header_unit "${unit}" \
                    "defined(BUILD_IMG2SIXEL) || defined(BUILD_SIXEL2PNG)"
                ;;
            src/loader-quicklook.h|src/loader-coregraphics.h|src/loader-wic.h)
                emit_header_unit "${unit}" "HAVE_QUICKLOOK"
                ;;
            include/sixel.h)
                public_header="${build_root}/include/sixel.h"
                if [ -f "${public_header}" ]; then
                    emit_header_unit "${public_header}" ""
                else
                    emit_header_unit "${unit}" ""
                fi
                ;;
            *)
                emit_header_unit "${unit}" ""
                ;;
        esac
    done
}

emit_all_units() {
    echo "${user_units}" | while IFS= read -r unit; do
        [ -z "${unit}" ] && continue

        case "${unit}" in
            src/threadpool.c)
                emit_unit "${unit}" "SIXEL_ENABLE_THREADS"
                ;;
            converters/cli.c|converters/completion_utils.c|converters/malloc_stub.c|converters/aborttrace.c)
                guard=$(echo "${unit}" | sed 's/.*\///;s/.c$//' | tr a-z\- A-Z_)
                emit_unit "${unit}" \
                    "defined(BUILD_IMG2SIXEL) || defined(BUILD_SIXEL2PNG) || defined(BUILD_${guard})"
                ;;
            converters/img2sixel.c)
                emit_unit "${unit}" "defined(BUILD_IMG2SIXEL)"
                ;;
            converters/sixel2png.c)
                emit_unit "${unit}" "defined(BUILD_SIXEL2PNG)"
                ;;
            assessment/lsqa.c)
                emit_unit "${unit}" "defined(BUILD_LSQA)"
                ;;
            src/tests.c)
                emit_unit "${unit}" "defined(BUILD_TESTS)"
                ;;
            tests/*.c)
                guard=$(echo "${unit}" | sed 's/.*\///;s/.c$//' | tr a-z\- A-Z_)
                emit_unit "${unit}" "defined(BUILD_${guard})"
                ;;
            src/clipboard_carbon.c)
                emit_unit "${unit}" \
                    "!defined(HAVE_APPKIT) || !defined(__OBJC__)"
                ;;
            src/quicklook_thumbnailing.m)
                emit_unit "${unit}" \
                    "HAVE_QUICKLOOK_THUMBNAILING && defined(__OBJC__)"
                ;;
            src/clipboard_macos.m)
                emit_unit "${unit}" \
                    "defined(HAVE_APPKIT) && defined(__OBJC__)"
                ;;
            *)
                emit_unit "${unit}" ""
                ;;
        esac
    done
}

collect_license_blocks

emit_config_header() {
    config_path="${build_root}/config.h"

    if [ ! -f "${config_path}" ]; then
        echo "config.h is missing at ${config_path}" >&2
        echo "Run configure to generate config.h before building amalgamation." \
            >&2
        exit 1
    fi

    printf "\n/* ==== config.h ==== */\n" >>"${output}"
    printf "#line 1 \"%s\"\n" "${config_path}" >>"${output}"
    awk '
        /^[ \t]*#pragma[ \t]+once[ \t]*$/ {
            print "/* #pragma once */";
            next;
        }
        { print; }
    ' "${config_path}" >>"${output}"
    printf "\n" >>"${output}"
}

cat >"${output}" <<EOF_HEADER
/*
 * SPDX-License-Identifier: MIT
 *
 * This file is generated by tools/gen-amalgamation.sh.
 * Do not edit manually. Enable the amalgamation option to rebuild it.
 */
EOF_HEADER

if [ -s "${license_temp}" ]; then
    {
        printf "/* ==== Combined licenses ==== */\n"
        cat "${license_temp}"
    } >>"${output}"
fi

cat >>"${output}" <<'EOF_MACROS'

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#if !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#define SIXEL_AMALGAMATION 1
EOF_MACROS
printf '\n' >>"${output}"
emit_config_header
emit_all_headers
emit_all_units
rm -f "${header_temp}" "${license_temp}" "${license_hash_temp}"
