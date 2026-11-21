#!/bin/sh
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
# Haiku slices require a built C test runner and a bounded spawn retry.
set -eux
echo '1..1'
set -v
awk '
function check(text,    n, lines, i, built, tested, line, strict, bounded) {
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        line = lines[i]
        sub(/^[[:space:]]+/, "", line)
        if (line ~ /^#/) continue
        if (line == "set -eux") strict = 1
        if (line == "meson compile -C builddir test_runner" && strict) built = 1
        if (line == "for attempt in 1 2; do") bounded = 1
        if (line ~ /^meson test / && line ~ /--no-rebuild/) {
            if (!built || !bounded) return 0
            ++tested
        }
    }
    return tested == 1
}
BEGIN {
    build = "set -eux\nmeson compile -C builddir test_runner"
    test_line = "meson test -C builddir --no-rebuild --num-processes 1\n"
    test = "for attempt in 1 2; do\n" test_line "done\n"
    if (!check(build "\n" test) || check(build "\n" test_line) ||
        check(test) || check(test build) ||
        check("meson compile -C builddir test_runner\n" test) ||
        check(build " || true\n" test)) {
        # The last control has a different command line, not a valid build.
        failed = 1
    }
}
/^    - name:/ {
    if (active && !check(source)) failed = 1
    active = /Build and test \(Meson, FreeBSD\/OpenBSD\/NetBSD\/Haiku\)/
    if (active) ++steps
    source = ""
    next
}
active { source = source $0 "\n" }
END {
    if (active && !check(source)) failed = 1
    if (steps != 1) failed = 1
    exit failed
}
' "$1/.github/actions/ci-steps/action.yml" || {
    echo 'not ok 1 - Haiku slices require built dependencies and a bounded retry'
    exit 0
}
echo 'ok 1 - Haiku slices require built dependencies and a bounded retry'
exit 0
