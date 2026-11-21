#!/bin/sh

set -eu

scriptdir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
case $(uname -s) in
  CYGWIN*|MINGW*|MSYS*)
    cd "$scriptdir"
    MSYS2_ARG_CONV_EXCL='*' exec cmd.exe /d /c example-build-wasi.cmd "$@"
    ;;
esac

exec python3 "$scriptdir/example-build-wasi.py" "$@"
