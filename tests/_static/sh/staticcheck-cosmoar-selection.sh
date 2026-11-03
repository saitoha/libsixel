#!/bin/sh
# Only Cosmopolitan Meson jobs select the response-file archive adapter.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v
src_root=$1
awk '
function valid(label, block) {
    if (label ~ /Meson-.*-cosmopolitan-/)
        return block ~ /AR=cosmoar-meson([[:space:]]|$)/
    return block !~ /AR=cosmoar-meson([[:space:]]|$)/
}
function finish() {
    if (label ~ /Meson-.*-cosmopolitan-/) ++checked
    if (!valid(label, block)) {
        print "# wrong archive adapter: " label
        failed = 1
    }
}
BEGIN {
    if (!valid("Meson-linux-cosmopolitan-static", "AR=cosmoar-meson") ||
        valid("Meson-linux-cosmopolitan-static", "AR=cosmoar") ||
        valid("Autotools-linux-cosmopolitan-static", "AR=cosmoar-meson") ||
        valid("Meson-linux-gcc", "AR=cosmoar-meson")) exit 1
}
/^[[:space:]]*- label:/ { finish(); label = $0; block = "" }
{ block = block "\n" $0 }
END { finish(); exit failed || checked == 0 }
' "$src_root/.github/workflows/ci.yml" >&2 || {
    echo 'not ok 1 - Cosmopolitan Meson archive adapter selection'
    exit 0
}
echo 'ok 1 - Cosmopolitan Meson archive adapter selection'
exit 0
