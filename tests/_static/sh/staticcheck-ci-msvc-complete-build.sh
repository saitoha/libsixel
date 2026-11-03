#!/bin/sh
# MSVC must compile non-default C tests before the fail-fast test rebuild.
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function check(text,    n, lines, i, line, builds, guard) {
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        line = lines[i]
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
        if (line == "" || line ~ /^rem /) continue
        if (guard) {
            if (line != "if errorlevel 1 exit /b !errorlevel!") return 0
            guard = 0
        }
        if (line ~ /^python -m mesonbuild[.]mesonmain compile /) {
            gsub(/[[:space:]]/, "", line)
            if (line != command && line != dist_command) return 0
            ++builds
            guard = 1
        }
    }
    return builds == 1 && !guard
}
BEGIN {
    good = "python -m mesonbuild.mesonmain compile -C builddir " \
        "test_runner --ninja-args=\"[\047-k\047, \0470\047, \047all\047]\""
    command = good
    gsub(/[[:space:]]/, "", command)
    dist_good = good
    # Only dist already compiles the runner in its extracted source tree.
    dist_target = "${{ fromJSON(inputs.matrix_json).check_target " \
        "!= \047dist\047 && \047test_runner\047 || \047\047 }}"
    runner_at = index(good, "test_runner")
    dist_good = substr(good, 1, runner_at - 1) dist_target \
        substr(good, runner_at + length("test_runner"))
    dist_command = dist_good
    gsub(/[[:space:]]/, "", dist_command)
    guard_line = "\nif errorlevel 1 exit /b !errorlevel!"
    no_runner = good
    sub(/test_runner /, "", no_runner)
    no_all = good
    sub(/, \047all\047/, "", no_all)
    stop_early = good
    sub(/\0470\047/, "\0471\047", stop_early)
    wrong_dist = dist_good
    sub(/!=/, "==", wrong_dist)
    if (!check(good guard_line) || !check(dist_good guard_line) ||
        check(wrong_dist guard_line) || check(no_runner guard_line) ||
        check(no_all guard_line) || check(stop_early guard_line) ||
        check(good) || check(good "\necho ignored" guard_line) ||
        check("rem " good guard_line)) {
        print "# MSVC complete-build scanner controls failed"
        failed = 1
    }
}
/^    - name:/ {
    if (active && !check(source)) failed = 1
    active = /Build and test \(Meson, Command Prompt\)/ ||
        /Rebuild and retest \(PGO use, Meson, Command Prompt\)/
    if (active) ++steps
    source = ""
    next
}
active { source = source $0 "\n" }
END {
    if (active && !check(source)) failed = 1
    if (steps != 2) failed = 1
    exit failed
}
' "$1/.github/actions/ci-steps/action.yml" || {
    echo 'not ok 1 - MSVC builds collect independent compiler failures'
    exit 0
}
echo 'ok 1 - MSVC builds collect independent compiler failures'
exit 0
