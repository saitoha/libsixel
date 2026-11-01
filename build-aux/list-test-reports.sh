#!/bin/sh
# Report files follow the same two category levels as their source cases.
set -eu
test -d "$1" || exit 1
LC_ALL=C
export LC_ALL
case ${2:-all} in
    trs) set -- "$1" "$1"/*.trs "$1"/*/*/*.trs ;;
    all) set -- "$1" "$1"/*.log "$1"/*.trs "$1"/*/*/*.log "$1"/*/*/*.trs ;;
    *) exit 1 ;;
esac
shift
for file do
    test -f "$file" || continue
    case ${file##*/} in staticcheck-*) continue ;; esac
    printf '%s\n' "$file"
done
