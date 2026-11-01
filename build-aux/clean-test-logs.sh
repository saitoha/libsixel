#!/bin/sh
# Remove reports before running tests, retaining staticcheck's separate owner.
set -eu
report_root=$1
host_os=${2-}
scope=${3:-runtime}
test -d "$report_root" || exit 1

# Recursive support discovery also removes reports left by renamed categories.
# Keep scan errors outside a pipeline, so partial discovery cannot succeed.
reports=$(sh "${0%/*}/list-tree-files.sh" "$report_root" '*.log' '*.trs')
printf '%s\n' "$reports" | {
    set --
    while IFS= read -r file; do
        test -n "$file" || continue
        case ${file##*/}:$scope in staticcheck-*:runtime) continue ;; esac
        case $host_os in
            *vms*)
                # RMS may reveal an earlier version after each deletion.
                while test -e "$file"; do rm -f "$file" || exit $?; done
                ;;
            *)
                set -- "$@" "$file"
                if test "$#" -eq 64; then
                    rm -f "$@" || exit $?
                    set --
                fi
                ;;
        esac
    done
    test "$#" -eq 0 || rm -f "$@"
}
