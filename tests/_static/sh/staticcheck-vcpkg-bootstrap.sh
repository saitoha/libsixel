#!/bin/sh
# Policy: docs/ci/design.md
# Policy: docs/testing/staticcheck.md
# Bound bootstrap retries and stop before dependency builds on failure.
set -eux
echo '1..1'
set -v
awk '
function valid(text,    retry, command, cleanup, finish, install, tail) {
    retry = index(text, "$bootstrapExit = 1\n        for ($attempt = 1; $attempt -le 3; ++$attempt) {")
    command = index(text, "cmd /c bootstrap-vcpkg.bat\n          $bootstrapExit = $LASTEXITCODE\n          if ($bootstrapExit -eq 0) { break }")
    cleanup = index(text, "Remove-Item -LiteralPath (Join-Path $vcpkgRoot \047vcpkg.exe\047) -Force -ErrorAction SilentlyContinue")
    finish = index(text, "if ($attempt -lt 3) { Start-Sleep -Seconds (5 * $attempt) }\n        }\n        if ($bootstrapExit -ne 0) { exit $bootstrapExit }\n        $packages =")
    install = index(text, ".\\\\vcpkg.exe install $packages --triplet x64-windows\n        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }")
    tail = text
    if (gsub(/cmd \/c bootstrap-vcpkg.bat/, "", tail) != 1) return 0
    if (gsub(/vcpkg.exe install/, "", tail) != 1) return 0
    return retry && command > retry && cleanup > command &&
        finish > cleanup && install > finish
}
/^    - name: Install vcpkg dependencies / { active = 1; next }
active && /^    - name:/ { active = 0 }
active { source = source $0 "\n" }
END {
    if (!valid(source)) {
        print "vcpkg must retry bootstrap only and propagate native failures"
        exit 1
    }
    for (mutation = 1; mutation <= 5; ++mutation) {
        changed = source
        if (mutation == 1) sub(/attempt -le 3/, "attempt -le 4", changed)
        if (mutation == 2) sub(/exit \$bootstrapExit/, "exit 0", changed)
        if (mutation == 3) sub(/Remove-Item/, "Write-Output", changed)
        if (mutation == 4) sub(/Start-Sleep/, "Write-Output", changed)
        if (mutation == 5) gsub(/exit \$LASTEXITCODE/, "exit 0", changed)
        if (changed == source || valid(changed)) {
            print "vcpkg bootstrap control escaped: " mutation
            exit 1
        }
    }
}' "$1/.github/actions/ci-steps/action.yml" >&2 || {
    echo 'not ok 1 - vcpkg bootstrap retries preserve failure propagation'
    exit 0
}
echo 'ok 1 - vcpkg bootstrap retries preserve failure propagation'
