#!/bin/sh
# Keep temporal method headers free from compat getenv declarations.

set -eu

echo "1..1"

src_root=${1:-}
header_path=
has_forbidden_symbol=0

if test -z "$src_root"; then
    echo "ok 1 # SKIP missing src root argument"
    exit 0
fi

header_path="$src_root/src/dither-interframe-method.h"
if test ! -f "$header_path"; then
    echo "ok 1 # SKIP missing temporal method header"
    exit 0
fi

# The compat getenv declaration must live only in compat_stub.h.
if awk '
    /^[[:space:]]*(\/\*|\*|\/\/)/ { next }
    /sixel_compat_getenv[[:space:]]*\(/ { found=1 }
    END { exit found ? 0 : 1 }
' "$header_path"; then
    has_forbidden_symbol=1
fi

if test "$has_forbidden_symbol" -ne 0; then
    echo "not ok 1 - temporal method header avoids compat getenv symbol"
    echo "# src/dither-interframe-method.h: remove sixel_compat_getenv symbol"
    exit 1
fi

echo "ok 1 - temporal method header avoids compat getenv symbol"
exit 0
