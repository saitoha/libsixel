#!/bin/sh
# Preserve owner-execute and regular-file observations using base utilities.
set -eu
# Re-enter with the filtered argv so AppleDouble metadata is ignored only after
# checking its signature; an ordinary shell source named ._* is still audited.
filter=0
for file do
    case "${file##*/}" in ._*) filter=1; break ;; esac
done
if test "$filter" = 0; then
    exec ls -ld "$@"
fi
count=$#
while test "$count" -gt 0; do
    file=$1
    shift
    count=$((count - 1))
    case "${file##*/}" in
        ._*)
            magic=$(od -An -N4 -tx1 "$file")
            case "$magic" in *'00 05 16 07'*) continue ;; esac
            ;;
    esac
    set -- "$@" "$file"
done
test "$#" -gt 0 || exit 0
ls -ld "$@"
