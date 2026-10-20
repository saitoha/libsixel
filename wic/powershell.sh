#!/bin/sh
# SPDX-License-Identifier: MIT
# Translate paths exactly once at the POSIX shell / native Windows boundary.
set -eu
powershell=$1
script=$2
shift 2
if command -v cygpath >/dev/null 2>&1; then
    script=$(cygpath -am "$script")
    count=$#
    path_argument=no
    while test "$count" -gt 0; do
        argument=$1
        shift
        if test "$path_argument" = yes; then
            argument=$(cygpath -am "$argument")
        fi
        path_argument=no
        case "$argument" in
            -Dll|-Output|-License|-Package|-Probe) path_argument=yes ;;
        esac
        set -- "$@" "$argument"
        count=$((count - 1))
    done
fi
MSYS2_ARG_CONV_EXCL='*' exec "$powershell" -NoProfile -NonInteractive \
    -ExecutionPolicy Bypass -File "$script" "$@"
