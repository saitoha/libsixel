#!/bin/sh
#
# Normalize OpenVMS/GNV compiler warning statuses for Autotools.
#
# The OpenVMS condition-value model can report a pure warning as a non-zero
# POSIX status even when the compiler wrote the requested object file.  GNU
# make treats that as fatal.  This wrapper preserves the compiler output and
# maps warning-only diagnostics back to success while keeping errors, fatal
# diagnostics, and gcc driver failures non-zero.

set -eu

prog=${0##*/}
cc_log=${TMPDIR:-.}/libsixel-gnv-cc-$$.log

trap 'rm -f "$cc_log"' 0 1 2 3 15

die()
{
    echo "$prog: $*" >&2
    exit 1
}

test "$#" -gt 0 || die "missing compiler command"
cc_cmd=$1
shift

set +e
"$cc_cmd" "$@" > "$cc_log" 2>&1
cc_status=$?
set -e

cat "$cc_log"

if test "$cc_status" -ne 0 &&
   { grep '^%[^-][^-]*-W-' "$cc_log" >/dev/null 2>&1 ||
     grep '[Ww]arning:' "$cc_log" >/dev/null 2>&1; } &&
   ! grep '^%[^-][^-]*-[EF]-' "$cc_log" >/dev/null 2>&1 &&
   ! grep '[Ee]rror:' "$cc_log" >/dev/null 2>&1 &&
   ! grep '^\?' "$cc_log" >/dev/null 2>&1; then
    exit 0
fi

exit "$cc_status"
