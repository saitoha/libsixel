#!/bin/sh
# Model the Windows RPC header that defines small as a type macro.
set -eux
echo '1..1'
set -v

src_root=$1
build_root=$2
cc_bin=${CC:-cc}
{ test -f "$build_root/config.h" && command -v "$cc_bin" >/dev/null 2>&1; } || {
    echo 'ok 1 # SKIP configured headers or C compiler unavailable'
    exit 0
}
"$cc_bin" -std=c99 -DHAVE_CONFIG_H -Dsmall=char -fsyntax-only \
    -I"$build_root" -I"$build_root/include" -I"$src_root/include" \
    "$src_root/src/refresh-spatial.c" || {
    echo 'not ok 1 - spatial refresh compiles with the RPC small macro'
    exit 0
}
echo 'ok 1 - spatial refresh compiles with the RPC small macro'
exit 0
