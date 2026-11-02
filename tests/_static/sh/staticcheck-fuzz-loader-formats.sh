#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Policy: docs/ci/design.md
set -eux
echo '1..1'
set -v
src_root=$1
checker=$src_root/tests/_static/awk/fuzz-loader-formats.awk
awk -f "$checker" "$src_root/Makefile.am" "$src_root/fuzz/Makefile.am" \
    "$src_root/fuzz/meson.build" "$src_root/.github/workflows/fuzzing.yml" >&2 || {
    echo 'not ok 1 - loader fuzz format registration'
    exit 0
}
# Each build inventory must independently detect a missing matrix target.
for inventory in Makefile.am fuzz/Makefile.am fuzz/meson.build; do
    {
        for source in Makefile.am fuzz/Makefile.am fuzz/meson.build .github/workflows/fuzzing.yml; do
            case "$source" in
                "$inventory") sed 's/webp/missing-format/g' "$src_root/$source" ;;
                *) cat "$src_root/$source" ;;
            esac
        done
    } | awk -f "$checker" >&2 && {
        echo 'not ok 1 - missing fuzz format escaped detection'
        exit 0
    }
done
echo 'ok 1 - loader fuzz format registration'
exit 0
