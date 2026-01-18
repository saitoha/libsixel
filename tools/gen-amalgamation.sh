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

collect_units_from_dirs() {
    search_dirs=$1
    shift
    units=""

    # Discovery rules:
    # - Use repository paths so downstream logic can resolve paths uniformly.
    # - Keep output deterministic via LC_ALL=C sort.
    # - Each directory group is appended in order to preserve a stable
    #   translation-unit layout.
    for dir in ${search_dirs}; do
        if [ ! -d "${src_root}/${dir}" ]; then
            continue
        fi
        found=$(find "${src_root}/${dir}" -type f "$@" | \
            LC_ALL=C sort | sed "s#^${src_root}/##")
        if [ -n "${found}" ]; then
            if [ -n "${units}" ]; then
                units="${units}
${found}"
            else
                units="${found}"
            fi
        fi
    done

    printf "%s\n" "${units}"
}

order_headers_by_includes() {
    awk -v src_root="${src_root}" -v build_root="${build_root}" '
function add_dep(parent, child) {
    key = parent SUBSEP child;
    if (!(key in deps)) {
        deps[key] = 1;
        indeg[child]++;
    }
}
function pick_file(path,    candidate) {
    candidate = src_root "/" path;
    if ((getline line < candidate) >= 0) {
        close(candidate);
        return candidate;
    }
    close(candidate);
    candidate = build_root "/" path;
    if ((getline line < candidate) >= 0) {
        close(candidate);
        return candidate;
    }
    close(candidate);
    return "";
}
BEGIN {
    count = 0;
}
NF {
    count++;
    paths[count] = $0;
    path_index[$0] = count;
    dirs[count] = $0;
    sub(/\/[^\/]+$/, "", dirs[count]);
    if (dirs[count] == paths[count]) {
        dirs[count] = "";
    }
    bases[count] = $0;
    sub(/^.*\//, "", bases[count]);
    base_count[bases[count]]++;
    base_path[bases[count]] = paths[count];
}
END {
    for (i = 1; i <= count; i++) {
        file = pick_file(paths[i]);
        if (file == "") {
            continue;
        }
        while ((getline line < file) > 0) {
            if (line ~ /^[ \t]*#[ \t]*include[ \t]*["<][^">]+[">]/) {
                inc = line;
                sub(/^[ \t]*#[ \t]*include[ \t]*["<]/, "", inc);
                sub(/[">].*$/, "", inc);
                dep = "";
                if (index(inc, "/") > 0 && (inc in path_index)) {
                    dep = inc;
                } else if (dirs[i] != "" &&
                           ((dirs[i] "/" inc) in path_index)) {
                    dep = dirs[i] "/" inc;
                } else if (("include/" inc) in path_index) {
                    dep = "include/" inc;
                } else if (base_count[inc] == 1 &&
                           (base_path[inc] in path_index)) {
                    dep = base_path[inc];
                }
                if (dep != "" && dep != paths[i]) {
                    add_dep(dep, paths[i]);
                }
            }
        }
        close(file);
    }

    processed_count = 0;
    while (processed_count < count) {
        candidate = "";
        for (i = 1; i <= count; i++) {
            path = paths[i];
            if (!(path in processed) && indeg[path] == 0) {
                if (candidate == "" || path < candidate) {
                    candidate = path;
                }
            }
        }
        if (candidate == "") {
            for (i = 1; i <= count; i++) {
                path = paths[i];
                if (!(path in processed)) {
                    if (candidate == "" || path < candidate) {
                        candidate = path;
                    }
                }
            }
        }
        processed[candidate] = 1;
        print candidate;
        for (key in deps) {
            split(key, parts, SUBSEP);
            if (parts[1] == candidate) {
                indeg[parts[2]]--;
            }
        }
        processed_count++;
    }
}
'
}

default_units=$(collect_units_from_dirs \
    "src tests converters assessment gdk-pixbuf-loader" \
    \( -name '*.c' -o -name '*.m' \))

header_units=$(collect_units_from_dirs \
    "include src converters tests" \
    \( -name '*.h' \))

# Ensure generated headers that may not exist in the source tree are still
# recognized by the amalgamation workflow.
header_units=$(printf "%s\n" \
    "include/sixel.h" \
    "${header_units}" \
    "converters/completion_embed.h" | \
    awk 'NF && !seen[$0]++')

existing_headers=""
missing_headers=""
while IFS= read -r header; do
    [ -z "${header}" ] && continue
    if [ -f "${src_root}/${header}" ] || [ -f "${build_root}/${header}" ]; then
        existing_headers="${existing_headers}
${header}"
    else
        missing_headers="${missing_headers}
${header}"
    fi
done <<EOF_HEADERS
${header_units}
EOF_HEADERS

header_units=$(printf "%s\n" "${existing_headers}" | \
    order_headers_by_includes)
header_units=$(printf "%s\n%s\n" "${header_units}" \
    "${missing_headers}" | awk 'NF && !seen[$0]++')

header_filter_units="${header_units}"
header_units=$(printf "%s\n" "${header_units}" | \
    awk '$0 != "src/stb_image.h" && $0 != "src/stb_image_write.h"')

# Persist header names to a temporary file so portable awk can read them
# without depending on vendor-specific split() behavior.
header_temp=$(mktemp "${TMPDIR:-/tmp}/sixel-headers.XXXXXX")
license_temp=$(mktemp "${TMPDIR:-/tmp}/sixel-licenses.XXXXXX")
license_hash_temp=$(mktemp "${TMPDIR:-/tmp}/sixel-license-hash.XXXXXX")
inline_state_temp=$(mktemp "${TMPDIR:-/tmp}/sixel-inline-once.XXXXXX")
trap 'rm -f "${header_temp}" "${license_temp}" \
    "${license_hash_temp}" "${inline_state_temp}"' EXIT
# Use the dynamically collected header list to build a basename registry for
# local-include filtering. The filter logic relies on filename matches rather
# than paths, so we collapse each entry to its final component.
printf "%s\n" "${header_filter_units}" | \
    awk -F/ 'NF { print $NF }' | LC_ALL=C sort -u >"${header_temp}"

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
    # Use octal escapes for the UTF-8 BOM so BSD awk stays compatible.
    sub(/^\357\273\277/, "");
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
    printf "%s\n" "${header_filter_units}" | while IFS= read -r unit; do
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
    inline_state=$4

    awk -v header_file="${header_file}" -v src_root="${src_root}" \
        -v inline_state="${inline_state}" '
BEGIN {
    while ((getline line < header_file) > 0) {
        gsub(/^[ \t]+|[ \t]+$/, "", line);
        if (length(line) > 0) {
            header[line] = 1;
        }
    }
    close(header_file);

    while ((getline line < inline_state) > 0) {
        gsub(/^[ \t]+|[ \t]+$/, "", line);
        if (length(line) > 0) {
            emitted[line] = 1;
        }
    }
    close(inline_state);

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
                print name >> inline_state;
                close(inline_state);
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
        "${src_root}" "${inline_state_temp}" >>"${output}"
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
        "${src_root}" "${inline_state_temp}" >>"${output}"
    printf "\n" >>"${output}"

    if [ -n "${guard_expr}" ]; then
        printf "#endif /* %s */\n" "${guard_expr}" >>"${output}"
    fi
}

emit_all_headers() {
    echo "${header_units}" | while IFS= read -r unit; do
        [ -z "${unit}" ] && continue

        case "${unit}" in
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
            gdk-pixbuf-loader/*.c|tests/*.c)
                guard=$(echo "${unit}" | \
                    sed 's/.*\///;s/.c$//' | tr a-z\- A-Z_)
                case "${guard}" in
                    [0-9]*)
                        guard="TEST_${guard}"
                        ;;
                esac
                emit_unit "${unit}" "defined(BUILD_${guard})"
                ;;
            src/threadpool.c)
                emit_unit "${unit}" "SIXEL_ENABLE_THREADS"
                ;;
            converters/cli.c|converters/completion_utils.c|converters/malloc_stub.c|converters/aborttrace.c)
                guard=$(echo "${unit}" | sed 's/.*\///;s/.c$//' | tr a-z\- A-Z_)
                emit_unit "${unit}" \
                    "defined(BUILD_IMG2SIXEL) || defined(BUILD_SIXEL2PNG) || defined(BUILD_${guard})"
                ;;
            converters/img2sixel.c|converters/sixel2png.c|assessment/lsqa.c)
                guard=$(echo "${unit}" | sed 's/.*\///;s/.c$//' | tr a-z\- A-Z_)
                emit_unit "${unit}" "defined(BUILD_${guard})"
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
