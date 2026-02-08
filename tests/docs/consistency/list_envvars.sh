#!/bin/sh
# Extract environment variable names from C sources and compare them with
# the variables advertised by img2sixel -H. This script relies only on
# POSIX sh, awk, sort, uniq, comm, and find.

set -eu

script_dir=$(CDPATH=; cd -- "$(dirname "$0")" && pwd)
repo_root=$(CDPATH=; cd -- "${script_dir}/../.." && pwd)

usage() {
    cat <<'USAGE'
Usage: tests/docs/list_envvars.sh [--check] [--img2sixel PATH]
                                  [--source-root DIR]

Options:
  --check           Return non-zero when a mismatch is detected.
  --img2sixel PATH   Path to the img2sixel binary built by Meson
                     (default: <repo>/build/converters/img2sixel).
  --source-root DIR  Repository root that contains the converters/src/
                     assessment/ trees (default: repository root).
USAGE
}

img2sixel=${repo_root}/build/converters/img2sixel${SIXEL_BIN_EXT-}
source_root=${repo_root}
check_only=0

while [ $# -gt 0 ]; do
    case "$1" in
        --img2sixel)
            shift || { usage >&2; exit 1; }
            img2sixel=$1
            ;;
        --source-root)
            shift || { usage >&2; exit 1; }
            source_root=$1
            ;;
        --check)
            check_only=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 1
            ;;
    esac
    shift
done

if [ ! -x "$img2sixel" ]; then
    echo "img2sixel binary not found: $img2sixel" >&2
    exit 1
fi

if [ ! -d "$source_root" ]; then
    echo "source root not found: $source_root" >&2
    exit 1
fi

# Gather environment variable names mentioned in the C sources. We limit the
# search to the primary code trees that feed the img2sixel build.
list_source_vars() {
    find "$source_root/converters" "$source_root/src" \
        "$source_root/assessment" -name '*.c' -print 2>/dev/null |
        LC_ALL=C sort |
        while IFS= read -r file; do
            awk '
                function emit(name) {
                    if (name ~ /^(SIXEL|IMG2SIXEL|LIBSIXEL)_[A-Z0-9_]+$/) {
                        print name
                    }
                }
                {
                    line = $0
                    while (match(line,
                                 /(getenv|sixel_helper_getenv_[A-Za-z0-9_]+)[ \t]*\([ \t]*"/)) {
                        start = RSTART + RLENGTH
                        rest = substr(line, start)
                        quote = index(rest, "\"")
                        if (quote == 0) {
                            break
                        }
                        emit(substr(rest, 1, quote - 1))
                        line = substr(rest, quote + 1)
                    }
                    line = $0
                    while (match(line,
                                 /"(SIXEL|IMG2SIXEL|LIBSIXEL)_[A-Z0-9_]+"/)) {
                        start = RSTART + 1
                        emit(substr(line, start, RLENGTH - 2))
                        line = substr(line, RSTART + RLENGTH)
                    }
                }
            ' "$file"
        done |
        LC_ALL=C sort -u
}

# Extract the environment variable names from the img2sixel -H output by
# reading the dedicated section.
list_help_vars() {
    img2sixel_dir=$(CDPATH=; cd -- "$(dirname "$img2sixel")" && pwd)
    runtime_var="${RUNTIME_SHLIBPATH_VAR:-LD_LIBRARY_PATH}"
    runtime_sep="${RUNTIME_SHLIBPATH_SEP:-:}"
    runtime_current=""
    shlibpath_overrides_runpath="${SIXEL_SHLIBPATH_OVERRIDES_RUNPATH:-yes}"
    if [ "$(basename "$img2sixel_dir")" = ".libs" ]; then
        build_root=$(CDPATH=; cd -- "$img2sixel_dir/../.." && pwd)
    else
        build_root=$(CDPATH=; cd -- "$img2sixel_dir/.." && pwd)
    fi
    runtime_libdir="$build_root/src/.libs"
    if [ -d "$runtime_libdir" ] &&
            [ "$shlibpath_overrides_runpath" = "yes" ]; then
        eval "runtime_current=\${${runtime_var}:-}"
        if [ -n "$runtime_current" ]; then
            runtime_value="$runtime_libdir$runtime_sep$runtime_current"
        else
            runtime_value="$runtime_libdir"
        fi
        eval "${runtime_var}=\${runtime_value}"
        eval "export ${runtime_var}"
    fi

    ${SIXEL_RUNTIME-} "$img2sixel" -H 2>/dev/null |
        awk '
            /^Environment variables:/ {
                in_env = 1
                next
            }
            in_env && NF == 0 {
                exit
            }
            in_env {
                if ($1 ~ /^(SIXEL|IMG2SIXEL|LIBSIXEL)_[A-Z0-9_]+$/) {
                    print $1
                }
            }
        ' |
        LC_ALL=C sort -u
}

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/list_envvars.XXXXXX") || exit 1
trap 'rm -rf "$tmpdir"' EXIT

source_list="$tmpdir/source.txt"
help_list="$tmpdir/help.txt"

list_source_vars >"$source_list"
list_help_vars >"$help_list"

missing_in_help=$(comm -23 "$source_list" "$help_list")
missing_in_source=$(comm -13 "$source_list" "$help_list")

printf 'C sources: %s entries\n' "$(wc -l <"$source_list")"
printf 'img2sixel -H: %s entries\n' "$(wc -l <"$help_list")"

printf '\nIn C sources only (missing from -H):\n'
if [ -n "$missing_in_help" ]; then
    printf '%s\n' "$missing_in_help"
else
    echo '(none)'
fi

printf '\nIn -H only (not found in sources):\n'
if [ -n "$missing_in_source" ]; then
    printf '%s\n' "$missing_in_source"
else
    echo '(none)'
fi

if [ "$check_only" -eq 1 ]; then
    if [ -n "$missing_in_help" ] || [ -n "$missing_in_source" ]; then
        echo "Mismatch detected between sources and -H output" >&2
        exit 1
    fi
fi
