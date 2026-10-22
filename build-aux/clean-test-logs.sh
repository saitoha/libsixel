#!/bin/sh
# Remove previous runtime results before launching any new tests. Staticcheck
# owns its separate reports, even when both suites share a build directory.
set -eu

case ${2-} in
  *vms*)
    # config.sub canonicalizes OpenVMS to vms, not openvms.
    # RMS may expose another version after each removal. Retain the GNV
    # per-path retry loop; batching on POSIX must not weaken this contract.
    find "$1" -type f \( -name '*.log' -o -name '*.trs' \) \
        ! -name 'staticcheck-*.log' ! -name 'staticcheck-*.trs' -print |
        while IFS= read -r file; do
            test -f "$file" || continue
            while test -e "$file"; do rm -f "$file" || exit $?; done
        done
    ;;
  *)
    # Let find bound argv for the host rather than launching rm per report
    # or expanding the complete inventory into one shell command. With '+',
    # a failed rm makes find fail too, preventing stale results being reused.
    exec find "$1" -type f \( -name '*.log' -o -name '*.trs' \) \
        ! -name 'staticcheck-*.log' ! -name 'staticcheck-*.trs' \
        -exec rm -f {} +
    ;;
esac
