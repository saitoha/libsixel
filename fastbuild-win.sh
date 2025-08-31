#!/bin/sh
#
# This shell script speeds up building libsixel on Cygwin and MSYS2.
# By using the pre-set parameters under config_sites/, it skips many of the
# checks performed by ./configure. This does trade off some of Autotools’
# strengths, but on my machine it reduced build time to roughly one third.
# If you want the safest build, use the standard ./configure instead.
#
# When mingw32-make (mingw-w64-*-make) is available, the script prefers it over make.
#
# Usage
# 
# ./fastbuild-win.sh [options to pass through to ./configure]
#

CONFIG_SHELL=$(which dash 2>/dev/null)
MAKE=$(which mingw32-make make 2>/dev/null | head -n1)

case "${MSYSTEM}${OSTYPE}" in
cygwin)
    CONFIG_SITE="/etc/config.site $(dirname "${0}")/config_sites/cygwin.site"
    ;;
MSYS*)
    CONFIG_SITE="/etc/config.site $(dirname "${0}")/config_sites/msys.site"
    ;;
MINGW64*)
    CONFIG_SITE="/etc/config.site $(dirname "${0}")/config_sites/mingw64.site"
    cc=gcc
    prefix=/mingw64
    host=x86_64-w64-mingw32
    ;;
UCRT64*)
    CONFIG_SITE="/etc/config.site $(dirname "${0}")/config_sites/ucrt64.site"
    cc=gcc
    prefix=/ucrt64
    host=x86_64-w64-mingw32
    ;;
CLANG64*)
    CONFIG_SITE="/etc/config.site $(dirname "${0}")/config_sites/clang64.site"
    cc=clang
    prefix=/clang64
    host=x86_64-w64-mingw32
    ;;
esac

export CONFIG_SHELL CONFIG_SITE

./configure --prefix="${prefix}" --host="${host}" CC="${cc}" "${@}" && \
    "${MAKE}" SHELL="${CONFIG_SHELL}" -j"$(nproc)"
