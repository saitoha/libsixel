#!/bin/sh
# A default instrumented study executable needs its own training profile.
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(text,    lines, n, i, command) {
    n = split(text, lines, "\n")
    for (i = 1; i <= n; ++i) {
        sub(/^[[:space:]]+/, "", lines[i])
        if (index(lines[i], "builddir\\tools\\progressive-study.exe " \
                "tests\\data\\inputs\\small.ppm") == 1) {
            sub(/^[[:space:]]+/, "", lines[i + 1])
            if (lines[i + 1] != "if errorlevel 1 exit /b %errorlevel%")
                return 0
            command++
        }
    }
    return command == 1 && index(text, "shell: cmd") &&
        index(text, "endsWith(fromJSON(inputs.matrix_json).label,")
}

/    - name:/ {
    if (/Rebuild and retest \(PGO use, Meson, Command Prompt\)/ && seen != 1) failed = 1
    if (active) {
        seen++
        missing = block
        sub(/progressive-study.exe/, "omitted", missing)
        unguarded = block
        sub(/if errorlevel 1 exit \/b %errorlevel%/, "", unguarded)
        commented = block
        sub(/        builddir/, "        rem builddir", commented)
        if (!valid(block) || valid(missing) || valid(unguarded) ||
                valid(commented)) failed = 1
    }
    active = /Train progressive study \(MSVC PGO\)/
    block = ""
}
active { block = block $0 "\n" }
END { exit failed || seen != 1 }
' "$1/.github/actions/ci-steps/action.yml" || {
    echo 'not ok 1 - MSVC PGO trains the default study executable'
    exit 0
}
echo 'ok 1 - MSVC PGO trains the default study executable'
