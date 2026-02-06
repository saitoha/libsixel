#!/bin/sh
# Shared helpers for converter TAP tests grouped by functionality.

# Enable tracing for diagnostics while preserving caller-specified -e/-u flags.
initial_shell_flags=$-
set -xv
case ${initial_shell_flags} in
*e*) set -e ;;
esac
case ${initial_shell_flags} in
*u*) set -u ;;
esac

conversion_common_path=${conversion_common_path:-"$0"}
conversion_helper_dir=${CONVERSION_HELPER_DIR-}
if [ -z "${conversion_helper_dir}" ]; then
    conversion_helper_dir=$(CDPATH=; cd "$(dirname "${conversion_common_path}")" && pwd)
fi
. "${conversion_helper_dir}/../common/tap.sh"
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
