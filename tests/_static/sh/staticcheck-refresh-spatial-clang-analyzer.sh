#!/bin/sh
# Treat Clang analyzer findings in the spatial map builder as failures.
set -eux
echo '1..1'
set -v

src_root=$1
build_root=$2
{ test -f "$build_root/config.h" && command -v clang >/dev/null 2>&1; } || {
    echo 'ok 1 # SKIP configured headers or Clang unavailable'
    exit 0
}
clang --analyze -std=c99 -DHAVE_CONFIG_H \
    -I"$build_root" -I"$build_root/include" -I"$src_root/include" \
    -Xanalyzer -analyzer-werror -Xanalyzer -analyzer-output=text \
    "$src_root/src/refresh-spatial.c" || {
    echo 'not ok 1 - Clang finds no defects in spatial refresh'
    exit 0
}
echo 'ok 1 - Clang finds no defects in spatial refresh'
exit 0
