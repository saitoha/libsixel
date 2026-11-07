#!/bin/sh
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
# Keep VM provisioning retries outside the single source build/test step.
set -eux
echo '1..1'
set -v
awk '
function valid(text,    n, blocks, i, b, first, second, reset, build) {
    n = split(text, blocks, "\n      - name:")
    for (i = 1; i <= n; ++i) {
        b = blocks[i]
        if (index(b, "id: sparc_prepare_")) {
            if (b ~ /run:/ || b ~ /configure|gmake -j|meson compile/ ||
                !index(b, "timeout-minutes: 25") ||
                !index(b, "uses: vmactions/netbsd-vm@") ||
                !index(b, "sync: scp")) return 0
            if (index(b, "id: sparc_prepare_1")) {
                if (first++ || !index(b, "continue-on-error: true") ||
                    index(b, "if:") ||
                    !index(b, "data-dir: ${{ runner.temp }}/sparc-vm-1")) return 0
            } else if (index(b, "id: sparc_prepare_2")) {
                if (second++ || !first || !reset ||
                    index(b, "continue-on-error:") ||
                    !index(b, "if: steps.sparc_prepare_1.outcome == '\''failure'\''") ||
                    !index(b, "disable-cache: true") ||
                    !index(b, "data-dir: ${{ runner.temp }}/sparc-vm-2")) return 0
            } else return 0
        } else if (index(b, "Reset failed SPARC VM before retry")) {
            if (reset++ || !first || second ||
                !index(b, "if: steps.sparc_prepare_1.outcome == '\''failure'\''") ||
                !index(b, "pkill -TERM") || !index(b, "/.ssh/config.d/*.conf") ||
                !index(b, "rm -f \"$config\"")) return 0
        } else if (index(b, "Build and test inside NetBSD/sparc64")) {
            if (build++ || !second || index(b, "continue-on-error:") ||
                !index(b, "if: steps.sparc_prepare_1.outcome == '\''success'\'' || steps.sparc_prepare_2.outcome == '\''success'\''") ||
                !index(b, "set -euo pipefail") ||
                !index(b, "netbsd sh -e <<'\''SIXEL_SPARC_BUILD'\''") ||
                !index(b, "gmake -j2 V=1 -C src") ||
                !index(b, "meson compile -C /tmp/libsixel-build") ||
                !index(b, "meson-tap-exitcode-wrapper.sh") ||
                b !~ /\n          SIXEL_SPARC_BUILD\n*$/) return 0
        } else if (b ~ /netbsd-vm@|sparc_prepare_/) return 0
    }
    return first == 1 && second == 1 && reset == 1 && build == 1
}
/^  sparc64-netbsd:/ { active = 1; next }
active && /^  [a-zA-Z0-9_-]+:/ { active = 0 }
active { source = source $0 "\n" }
END {
    if (!valid(source)) {
        print "SPARC must retry only preparation and propagate the single build status"
        exit 1
    }
    # These controls must be rejected without starting a VM or a compiler.
    for (mutation = 1; mutation <= 7; ++mutation) {
        changed = source
        if (mutation == 1) gsub(/outcome == '\''failure'\''/, "outcome != '\''success'\''", changed)
        if (mutation == 2) sub(/disable-cache: true/, "disable-cache: false", changed)
        if (mutation == 3) sub(/sparc-vm-2/, "sparc-vm-1", changed)
        if (mutation == 4) sub(/netbsd sh -e/, "netbsd sh", changed)
        if (mutation == 5) changed = changed "          true\n"
        if (mutation == 6) sub(/prepare: /, "run: ", changed)
        if (mutation == 7) sub(/pkill -TERM/, "echo", changed)
        if (changed == source || valid(changed)) {
            print "SPARC retry removal control escaped: " mutation
            exit 1
        }
    }
}' "$1/.github/workflows/experimental.yml" >&2 || {
    echo 'not ok 1 - SPARC retries only VM preparation'
    exit 0
}
echo 'ok 1 - SPARC retries only VM preparation'
