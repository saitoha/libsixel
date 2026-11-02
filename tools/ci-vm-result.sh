#!/bin/sh
# Preserve guest failures as data until the VM action copies artifacts back.
# See docs/ci/design.md for the infrastructure/artifact boundary.
set -eu
case ${1-} in
    capture)
        # A fresh file belongs to this attempt. Do not execute the build if
        # the result cannot be replaced; an interrupted child leaves it empty.
        : > "$2"
        status=0
        sh -e || status=$?
        printf '%s\n' "$status" > "$2"
        ;;
    check)
        status=
        extra=
        # Read exactly one canonical exit-status line. Missing, partial and
        # malformed records fail closed, including exhausted VM retries.
        test -n "${2-}" && test -f "$2" || {
            echo 'VM did not return a build result' >&2
            exit 1
        }
        {
            IFS= read -r status && ! IFS= read -r extra && test -z "$extra"
        } < "$2" || status=invalid
        case $status in
            0|[1-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5]) ;;
            *) echo 'VM returned an invalid build result' >&2; exit 1 ;;
        esac
        echo "VM build/test exit status: $status"
        exit "$status"
        ;;
    *) echo "Usage: $0 capture|check RESULT_FILE" >&2; exit 2 ;;
esac
