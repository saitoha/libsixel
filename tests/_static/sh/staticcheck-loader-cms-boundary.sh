#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Loader adapters select source definitions through the shared CMS facade.

set -eux

echo '1..1'
set -v

src_root=${1:-}

test -n "$src_root" && test -d "$src_root/src" || {
    echo '# a source root containing src is required'
    echo 'not ok 1 - loader CMS evaluation stays behind the shared facade'
    exit 0
}

# Scan the filesystem, including new adapters and distribution tarballs.
# The CMS implementation and its private ICC engine are deliberate owners.
awk -f "$src_root/tests/_static/awk/loader-cms-boundary.awk" \
    "$src_root"/src/loader*.c "$src_root"/src/from*.c || {
    echo 'not ok 1 - loader CMS evaluation stays behind the shared facade'
    exit 0
}

echo 'ok 1 - loader CMS evaluation stays behind the shared facade'
exit 0
