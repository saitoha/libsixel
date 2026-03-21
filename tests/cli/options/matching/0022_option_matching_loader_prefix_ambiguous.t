#!/bin/sh
# TAP test verifying ambiguous -L loader prefixes are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

find_ambiguous_prefix() {
    names="$1"
    for name in ${names}; do
        len=${#name}
        i=1
        while [ "${i}" -lt "${len}" ]; do
            prefix=$(printf '%s' "${name}" | cut -c1-"${i}")
            count=0
            for other in ${names}; do
                case "${other}" in
                    "${prefix}"*)
                        count=$((count + 1))
                        ;;
                    *)
                        ;;
                esac
            done
            if [ "${count}" -ge 2 ]; then
                printf '%s' "${prefix}"
                return 0
            fi
            i=$((i + 1))
        done
    done
    return 1
}

loader_names="builtin"
test "${HAVE_LIBPNG-}" = 1 && loader_names="${loader_names} libpng"
test "${HAVE_JPEG-}" = 1 && loader_names="${loader_names} libjpeg"
test "${HAVE_WEBP-}" = 1 && loader_names="${loader_names} libwebp"
test "${HAVE_LIBTIFF-}" = 1 && loader_names="${loader_names} libtiff"
test "${HAVE_LIBRSVG-}" = 1 && loader_names="${loader_names} librsvg"
test "${HAVE_WIC-}" = 1 && loader_names="${loader_names} wic"
test "${HAVE_COREGRAPHICS-}" = 1 && loader_names="${loader_names} coregraphics"
test "${HAVE_GDK_PIXBUF2-}" = 1 && loader_names="${loader_names} gdk-pixbuf2"
test "${HAVE_GD-}" = 1 && loader_names="${loader_names} gd"
test "${HAVE_COREGRAPHICS-}" = 1 && test "${HAVE_QUICKLOOK-}" = 1 &&
    loader_names="${loader_names} quicklook"
test "${HAVE_FREEDESKTOP_THUMBNAILING-}" = 1 &&
    loader_names="${loader_names} gnome-thumbnailer"

ambiguous_prefix=$(find_ambiguous_prefix "${loader_names}" || true)
test -n "${ambiguous_prefix}" || {
    printf "1..0 # SKIP no ambiguous -L prefixes in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel -L"${ambiguous_prefix}" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "ambiguous -L prefix unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"ambiguous prefix"*)
        ;;
    *)
        echo "not ok" 1 - "missing ambiguous -L prefix diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "ambiguous -L prefix is rejected"
exit 0
