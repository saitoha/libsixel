#!/bin/sh
# Enumerate source trees with the host shell; Python is not a host prerequisite.
set -eu
# Source identifiers and path separators are byte-oriented. Avoid quadratic
# multibyte substring traversal in BSD awk on large continued source lists.
LC_ALL=C
export LC_ALL
names=
first_name=
name_count=0
include_links=0
excludes=
excluded_paths=
roots=
max_depth=-1
separator='\n'
newline='
'
while test "$#" -gt 0; do
    case "$1" in
        --name)
            names=$names$newline$2
            test -n "$first_name" || first_name=$2
            name_count=$((name_count + 1)); shift 2 ;;
        --exclude-name) excludes=$excludes$newline$2; shift 2 ;;
        --exclude-path) excluded_paths=$excluded_paths$newline$2; shift 2 ;;
        --max-depth) max_depth=$2; shift 2 ;;
        --null) separator='\0'; shift ;;
        --include-links) include_links=1; shift ;;
        --exec) shift; break ;;
        --*) echo "source inventory: unknown option $1" >&2; exit 1 ;;
        *) roots=$roots$newline$1; shift ;;
    esac
done
case "$max_depth" in -1) ;; ''|*[!0-9]*) exit 1 ;; esac
test -n "$roots" || { echo 'source inventory: no roots' >&2; exit 1; }

# Pattern lists are split only on newlines, without expanding them in the cwd.
matches() {
    set -f
    IFS=$newline
    for pattern in $2; do
        # The caller supplies shell glob patterns, not literal names.
        # shellcheck disable=SC2254
        case "$1" in $pattern) set +f; return 0 ;; esac
    done
    set +f
    return 1
}
walk() {
    test -d "$1" && test -r "$1" && test -x "$1" || {
        echo "source inventory: unreadable directory $1" >&2
        return 1
    }
    # Function positional parameters retain the parent's path/depth across
    # recursion. Hidden directories are pruned; hidden regular files remain.
    for entry in "$1"/* "$1"/.[!.]* "$1"/..?*; do
        test "$include_links" -eq 1 || test ! -L "$entry" || continue
        test -z "$excluded_paths" || { matches "$entry" "$excluded_paths" && continue; }
        if test -d "$entry" && test ! -L "$entry"; then
            case "${entry##*/}" in .*|_artifacts) continue ;; esac
            if test "$max_depth" -lt 0 || test "$2" -lt "$max_depth"; then
                walk "$entry" "$(( $2 + 1 ))" || return 1
            fi
        elif test -f "$entry" || test -L "$entry"; then
            if test "$name_count" -gt 0; then
                # The common one-pattern case needs no function or list split.
                # shellcheck disable=SC2254
                case "${entry##*/}" in
                    $first_name) ;;
                    *)
                        test "$name_count" -gt 1 || continue
                        matches "${entry##*/}" "$names" || continue
                        ;;
                esac
            fi
            test -z "$excludes" || { matches "${entry##*/}" "$excludes" && continue; }
            printf '%s%b' "$entry" "$separator"
        fi
    done
}
inventory() {
    set -f
    IFS=$newline
    # Split the newline-separated roots with pathname expansion disabled.
    # shellcheck disable=SC2086
    set -- $roots
    set +f
    for root do
        if test "$include_links" -eq 1; then
            walk "$root" 1 || return 1
            continue
        fi
        absolute=$root
        case "$absolute" in /*) ;; *) absolute=$PWD/$root ;; esac
        case "$absolute" in
            "${SIXEL_STATICCHECK_INVENTORY_ROOT:-/nonexistent-staticcheck-inventory}"|"${SIXEL_STATICCHECK_INVENTORY_ROOT:-/nonexistent-staticcheck-inventory}"/*)
                test -r "$root" && test -d "$root" && test -x "$root" || return 1
                nul=0
                test "$separator" != '\0' || nul=1
                SIXEL_SCAN_NAMES=$names SIXEL_SCAN_EXCLUDES=$excludes \
                    SIXEL_SCAN_PATHS=$excluded_paths \
                    awk -v absolute="$absolute" -v requested="$root" \
                    -v max_depth="$max_depth" \
                    -v nul="$nul" -f "${0%/*}/../awk/source-inventory-select.awk" \
                    "$SIXEL_STATICCHECK_TEST_INVENTORY" || return 1
                ;;
            *) walk "$root" 1 || return 1 ;;
        esac
    done
}
# Build the inventory before invoking tools so a scan failure cannot be
# hidden by the last command. xargs preserves argv boundaries with NUL records
# and bounds command size, including on hosts with small argument limits.
if test "$#" -eq 0; then
    inventory
else
    manifest=$(mktemp "${TMPDIR:-/tmp}/sixel-sources-XXXXXX")
    trap 'rm -f "$manifest"' EXIT HUP INT TERM
    separator='\0'
    inventory > "$manifest" || exit 1
    test -s "$manifest" || exit 0
    xargs -0 -n 1024 -s 24000 "$@" < "$manifest"
fi
