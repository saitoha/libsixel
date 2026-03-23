#!/bin/sh
# Extract environment variable names from C sources and compare them with
# the variables listed in converters/img2sixel.c:g_env_help_table.
# This static check relies only on POSIX sh, awk, sort, uniq, comm, and find.

set -eu

script_dir=$(CDPATH=; cd -- "$(dirname "$0")" && pwd)
repo_root=$(CDPATH=; cd -- "${script_dir}/../.." && pwd)

usage() {
    cat <<'USAGE'
Usage: tests/docs/consistency/list_envvars.sh [--check]
                                              [--source-root DIR]
                                              [--help-source PATH]

Options:
  --check           Return non-zero when a mismatch is detected.
  --source-root DIR  Repository root that contains the converters/src/
                     assessment/ trees (default: repository root).
  --help-source PATH  Path to img2sixel.c that defines g_env_help_table
                      (default: <source-root>/converters/img2sixel.c).
USAGE
}

source_root=${repo_root}
help_source=
check_only=0

while [ $# -gt 0 ]; do
    case "$1" in
        --help-source)
            shift || { usage >&2; exit 1; }
            help_source=$1
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

if [ ! -d "$source_root" ]; then
    echo "source root not found: $source_root" >&2
    exit 1
fi

if [ -z "$help_source" ]; then
    help_source="$source_root/converters/img2sixel.c"
fi

if [ ! -f "$help_source" ]; then
    echo "img2sixel help source not found: $help_source" >&2
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

# Extract environment variable names from converters/img2sixel.c by reading
# g_env_help_table entries.
list_help_vars() {
    awk '
            /^[[:space:]]*static[[:space:]]+cli_env_help_t[[:space:]]+const[[:space:]]+g_env_help_table\[\][[:space:]]*=[[:space:]]*\{/ {
                in_env = 1
                next
            }
            in_env && /^[[:space:]]*};/ {
                exit
            }
            in_env && /^[[:space:]]*\{[[:space:]]*$/ {
                want_name = 1
                next
            }
            in_env && want_name {
                line = $0
                if (match(line, /"(SIXEL|IMG2SIXEL|LIBSIXEL)_[A-Z0-9_]+"/)) {
                    print substr(line, RSTART + 1, RLENGTH - 2)
                    want_name = 0
                }
            }
        ' "$help_source" |
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
printf 'img2sixel env help table: %s entries\n' "$(wc -l <"$help_list")"

printf '\nIn C sources only (missing from help table):\n'
if [ -n "$missing_in_help" ]; then
    printf '%s\n' "$missing_in_help"
else
    echo '(none)'
fi

printf '\nIn help table only (not found in sources):\n'
if [ -n "$missing_in_source" ]; then
    printf '%s\n' "$missing_in_source"
else
    echo '(none)'
fi

if [ "$check_only" -eq 1 ]; then
    if [ -n "$missing_in_help" ] || [ -n "$missing_in_source" ]; then
        echo "Mismatch detected between sources and img2sixel env help table" >&2
        exit 1
    fi
fi
