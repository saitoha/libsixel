#!/bin/sh
# Execute one batch, then publish independent successful per-file results.
set -eu
item_id=$$
log_path=$SHELLCHECK_LOG_DIR/$item_id.log

# A failed batch publishes nothing. The pending list never contains the other
# fresh files, so one changed failure cannot invalidate their successful results.
if ! "$SHELLCHECK_CMD_INNER" -x -P "$REPO_ROOT_INNER" "$@" > "$log_path" 2>&1; then
    printf 'NG\t%s\t%s\n' "$item_id" "$#"
    exit 0
fi
count=$#
if test -n "$SHELLCHECK_CACHE_ROOT"; then
    # Stage on the destination filesystem: cross-device mv is a copy, not an
    # atomic rename. A unique directory also isolates simultaneous invocations.
    batch_dir=$(mktemp -d "$SHELLCHECK_CACHE_ROOT/.pending-XXXXXX")
    trap 'rm -rf "$batch_dir"' EXIT
    trap 'exit 1' HUP INT TERM
    # Queue order groups siblings. touch and mv accept several files at once;
    # each result is still renamed atomically. Backdating to analysis start
    # prevents an edit during the check from acquiring a fresh cached result.
    publish() {
        test "$#" -ne 0 || return 0
        mkdir -p "$destination"
        touch -r "$SHELLCHECK_LOG_DIR/cache-started" "$@"
        mv "$@" "$destination/"
    }
    destination=
    # POSIX for captures the original list before set changes the arguments.
    # The new argument list contains only this directory's result artifacts.
    for source do
        relative=${source#"$REPO_ROOT_INNER/"}
        next_destination=$SHELLCHECK_CACHE_ROOT/${relative%/*}
        if test "$next_destination" != "$destination"; then
            test -z "$destination" || publish "$@"
            set --
            destination=$next_destination
        fi
        result=$batch_dir/${source##*/}.trs
        printf ':test-result: PASS\n' > "$result"
        set -- "$@" "$result"
    done
    publish "$@"
fi
printf 'OK\t%s\t%s\n' "$item_id" "$count"
