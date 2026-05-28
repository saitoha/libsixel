#!/bin/sh
#
# Normalize OpenVMS/GNV archive warning statuses for Autotools.
#
# The GNV ar wrapper uses the native OpenVMS LIBRARY command underneath.  When
# an object module records compiler warnings, LIBRARY can emit %LIBRAR-W-COMCOD
# and return a warning status even though it has updated the archive.  POSIX
# make treats that non-zero status as a hard failure, so translate pure LIBRARY
# warnings back to success while preserving real error and fatal diagnostics.

set -eu

prog=${0##*/}
ar_log=${TMPDIR:-.}/libsixel-gnv-ar-$$.log

trap 'rm -f "$ar_log"' 0 1 2 3 15

die()
{
    echo "$prog: $*" >&2
    exit 1
}

test "$#" -gt 0 || die "missing archiver command"
ar_cmd=$1
shift

set +e
"$ar_cmd" "$@" > "$ar_log" 2>&1
ar_status=$?
set -e

cat "$ar_log"

if test "$ar_status" -eq 2 &&
   grep '^%LIBRAR-W-' "$ar_log" >/dev/null 2>&1 &&
   ! grep '^%[^-][^-]*-[EF]-' "$ar_log" >/dev/null 2>&1; then
    exit 0
fi

exit "$ar_status"
