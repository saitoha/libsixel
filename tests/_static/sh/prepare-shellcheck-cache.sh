#!/bin/sh
# Select stale per-file results with Make, without starting a shell per rule.
set -eu
root=$1
inventory=$2
work=$3
helper_dir=${0%/*}
: > "$work/cache-root"
: > "$work/cache-hits"

# Custom options/configuration may enable sources outside the repository.
# Keep that path uncached rather than inventing a ShellCheck dependency parser.
enabled=${STATICCHECK_CACHE:-1}
test -z "${SHELLCHECK_OPTS:-}" || enabled=0
cache_make=${SIXEL_STATICCHECK_MAKE:-make}
command -v "$cache_make" >/dev/null 2>&1 || enabled=0
if command -v sha256sum >/dev/null 2>&1; then
    set -- sha256sum
elif command -v shasum >/dev/null 2>&1; then
    set -- shasum -a 256
else
    enabled=0
fi
# Keep mutable artifacts outside tests/: parallel source scans traverse that
# tree and must not encounter staging directories being removed by workers.
cache_base=${TOP_BUILDDIR:-$root}/.staticcheck-cache/shellcheck
# Make's filename syntax is deliberately narrower than the execution queue.
# Spaces are escaped below; unusual paths still receive a fresh check.
case "$root$cache_base" in *[!a-zA-Z0-9_./\ +-]*) enabled=0 ;; esac
test -s "$inventory" || enabled=0
if test "$enabled" != 0; then
    awk '
    {
        dir = $0
        sub(/\/[^\/]*$/, "", dir)
        while (dir != "" && !seen[dir]++) {
            print dir
            sub(/\/[^\/]*$/, "", dir)
        }
    }
    END { print "/" }
    ' "$inventory" > "$work/directories"
    while IFS= read -r dir; do
        if test -e "$dir/.shellcheckrc" || test -e "$dir/shellcheckrc"; then
            enabled=0
            break
        fi
    done < "$work/directories"
    for config in "${HOME:-}/.shellcheckrc" \
        "${XDG_CONFIG_HOME:-${HOME:-}/.config}/shellcheckrc" \
        "${APPDATA:-${HOME:-}/.config}/shellcheckrc"; do
        test ! -e "$config" || enabled=0
    done
fi
if test "$enabled" = 0; then
    awk '{ printf "%s%c", $0, 0 }' "$inventory" > "$work/queue.bin"
    printf '0\n' > "$work/cache-hits"
    exit 0
fi

# Some supported Make versions compare only whole seconds. A result stamped
# one second before this run cannot hide an edit in the same clock tick. Very
# recent inputs may therefore be checked again until the next second; prefer
# that bounded extra work over a false hit. One shared timestamp serves all
# batches, including on platforms whose touch drops subsecond precision.
started=$(date +%s)
started=$((started - 1))
stamp=$(date -u -r "$started" '+%Y%m%d%H%M.%S' 2>/dev/null) ||
    stamp=$(date -u -d "@$started" '+%Y%m%d%H%M.%S')
TZ=UTC0 touch -t "$stamp" "$work/cache-started"

# The namespace binds results to the executable bytes, invocation context and
# cache implementation. No mutable shared manifest or batch identity is needed.
tool=$(command -v "${SHELLCHECK_CMD:-shellcheck}")
{
    printf '%s\n' "$root" "$PWD" '-x -P' \
        "${ARTIFACT_LOCAL_DIR:-}" "${TOP_SRCDIR:-}" \
        "${LANG:-}" "${LC_ALL:-}" "${LC_CTYPE:-}"
    "$tool" --version
    "$@" "$tool" "$helper_dir/shellcheck.sh" "$0" \
        "$helper_dir/run-shellcheck-batch.sh" "$helper_dir/run-check-batches.sh"
} > "$work/context"
key=$("$@" < "$work/context")
key=${key%% *}
cache_root=$cache_base/$key
printf '%s\n' "$cache_root" > "$work/cache-root"

# Only self-contained inputs can be reused. This is a conservative lexical
# exclusion, not a shell parser: even comments containing source commands can
# cause a fresh check. Remove quoting/continuations to catch split spellings;
# encoded ANSI-C words are always excluded. External-source directives also
# contain "source". Unknown/dynamic commands are not followed by ShellCheck.
CACHE_ROOT=$cache_root CACHE_SOURCE_ROOT=$root awk '
function escaped(path) {
    gsub(/ /, "\\ ", path)
    return path
}
BEGIN {
    root = ENVIRON["CACHE_SOURCE_ROOT"]
    cache = ENVIRON["CACHE_ROOT"]
    print ".PHONY: all force\nall:\nforce:"
}
{
    path = $0
    relative = substr(path, length(root) + 2)
    result = cache "/" relative ".trs"
    eligible = path !~ /[^a-zA-Z0-9_.\/ +\-]/
    body = ""
    while ((status = getline line < path) > 0) body = body line "\n"
    if (status < 0) exit 1
    close(path)
    if (body ~ /\$[\047"]/) eligible = 0
    gsub(/\\\n/, "", body)
    gsub(/[\047"\\]/, "", body)
    if (index(body, "source") ||
        body ~ /(^|[^[:alnum:]_.\/-])\.([^[:alnum:]_.\/-]|$)/)
        eligible = 0
    status = getline line < result
    valid = status > 0 && line == ":test-result: PASS"
    if (status >= 0) {
        valid = (getline line < result) == 0 && valid
        close(result)
    }
    target = eligible ? escaped(result) : "uncached-" NR
    print "all: " target
    print target ": " (eligible ? escaped(path) : "") (valid && eligible ? "" : " force")
    print "\t@echo SHELLCHECK_STALE " NR
}
' "$inventory" > "$work/Makefile"

# Dry-run output is data, never shell code. Each recipe contains only its
# numeric inventory index; Make retains its normal timestamp and -B semantics.
"$cache_make" --no-print-directory -s -n -f "$work/Makefile" all > "$work/stale"
awk -v hits="$work/cache-hits" '
FILENAME == ARGV[1] {
    if (NF != 3 || $1 != "echo" || $2 != "SHELLCHECK_STALE" ||
        $3 !~ /^[1-9][0-9]*$/ || stale[$3]++) bad = 1
    next
}
FNR in stale { printf "%s%c", $0, 0; pending++ }
END {
    for (id in stale) if (id + 0 > FNR) bad = 1
    if (bad) exit 1
    print FNR - pending > hits
}
' "$work/stale" "$inventory" > "$work/queue.bin"
