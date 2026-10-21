#!/bin/sh
# Schedule NUL-delimited argument batches through the invoking make's jobserver.
set -eu

# A recursive recipe must stay visible to Make's jobserver, including through
# the TAP driver. Its dry run must stop before that driver creates reports.
mode=batches
if test "${1:-}" = --make-command; then
    mode='command'
    SIXEL_STATICCHECK_MAKE=$2
    export SIXEL_STATICCHECK_MAKE
    shift 2
fi
if test -n "${SIXEL_STATICCHECK_MAKE:-}"; then
    # GNU Make 3.81 can retain grouped flags such as -sn after long options.
    # Inspect only boolean short flags, stopping before command assignments;
    # do not mistake --no-print-directory or an include path for dry-run -n.
    set -f
    first_flag=yes
    for make_flag in ${MAKEFLAGS:-}; do
        allow_bare=$first_flag
        first_flag=no
        case "$make_flag" in
            --) break ;;
            --just-print|--dry-run|--recon) exit 0 ;;
            --*) continue ;;
            -*) ;;
            *) test "$allow_bare" = yes || continue ;;
        esac
        make_flag=${make_flag#-}
        case "$make_flag" in
            ''|*[!bBdeiknpqrRsStw]*) continue ;;
            *n*) exit 0 ;;
        esac
    done
fi
if test "$mode" = command; then
    exec "$@"
fi

batch_size=$1
queue=$2
shift 2

# Direct and Meson invocations have no Make jobserver. Never invent a second
# CPU budget; xargs without -P executes all batches serially on these paths.
if test -z "${SIXEL_STATICCHECK_MAKE:-}"; then
    exec xargs -0 -n "$batch_size" "$@" < "$queue"
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-check-batches-XXXXXX")
# shellcheck disable=SC2329
cleanup() {
    # Foreground Make reaps its recipes before the shell runs this trap.
    rm -rf "$tmpdir"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

# Keep command arguments out of Make syntax. The shell quoting also preserves
# literal dollars, quotes, whitespace and multiline sh -c programs.
{
    printf 'exec '
    for argument do
        printf "'"
        # Quote literal apostrophes with parameter expansion and printf only;
        # invoking sed once per argument would add processes to every batch.
        while :; do
            case "$argument" in
                *"'"*)
                    printf '%s%s' "${argument%%"'"*}" "'\\''"
                    argument=${argument#*"'"}
                    ;;
                *) printf '%s' "$argument"; break ;;
            esac
        done
        printf "' "
    done
    printf '"$@"\n'
} > "$tmpdir/command.sh"

SIXEL_STATICCHECK_QUEUE_DIR=$tmpdir
export SIXEL_STATICCHECK_QUEUE_DIR
printf '.PHONY: all\nall:\n' > "$tmpdir/Makefile"
# Preparation is serial. Each generated recipe receives exactly one batch,
# while recursive recipe markers preserve jobserver descriptors for nested
# lint schedulers. No -j option is supplied: Make owns the shared job budget.
# shellcheck disable=SC2016
xargs -0 -n "$batch_size" sh -c '
    printf "%s\000" "$@" > "$SIXEL_STATICCHECK_QUEUE_DIR/$$.args"
    printf ".PHONY: batch-%s\nall: batch-%s\nbatch-%s:\n\t+@xargs -0 sh \"\$\$SIXEL_STATICCHECK_QUEUE_DIR/command.sh\" < \"\$\$SIXEL_STATICCHECK_QUEUE_DIR/%s.args\"\n" \
        "$$" "$$" "$$" "$$"
' sh < "$queue" >> "$tmpdir/Makefile"

status=0
"$SIXEL_STATICCHECK_MAKE" --no-print-directory -s -k \
    -f "$tmpdir/Makefile" all || status=$?
exit "$status"
