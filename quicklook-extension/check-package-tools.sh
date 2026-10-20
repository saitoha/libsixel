#!/bin/sh
# Both configure frontends share the distribution version/tool contract.
set -eu
version=${1:?missing distribution version}
source_version=${2:?missing source version}
for value in "$version" "${source_version%%-*}"; do
    remainder=$value
    for limit in 9999 99 99; do
        part=${remainder%%.*}
        case "$part" in
            ''|*[!0-9]*|0[0-9]*)
                echo "Quick Look package version requires canonical M.m.p" >&2
                exit 1 ;;
        esac
        test "${#part}" -le 4 && test "$part" -le "$limit" || {
            echo "Quick Look package version component is out of range" >&2
            exit 1
        }
        test "$limit" -ne 9999 || test "$part" -gt 0 || {
            echo "Quick Look package major version must be positive" >&2
            exit 1
        }
        remainder=${remainder#*.}
    done
    # Reconstructing the last two components rejects both missing and extra dots.
    tail=${value#*.}
    test "$value" = "${value%%.*}.${tail%%.*}.$part" || {
        echo "Quick Look package version requires exactly three components" >&2
        exit 1
    }
done
for tool in /usr/bin/pkgbuild /usr/bin/productbuild /usr/sbin/pkgutil \
    /usr/bin/codesign /usr/bin/xcrun /usr/bin/lsbom; do
    test -x "$tool" || {
        echo "Quick Look packaging requires macOS/Xcode tool: $tool" >&2
        exit 1
    }
done
for tool in lipo otool; do
    /usr/bin/xcrun --find "$tool" >/dev/null || {
        echo "Quick Look packaging requires Xcode CLT tool: $tool" >&2
        exit 1
    }
done
