#!/bin/sh
# Keep architecture and compiler support evidence on the full test suite.
# Policy: docs/ci/design.md

set -eux

src_root=$1
workflow=$src_root/.github/workflows/experimental.yml

echo "1..1"
set -v

test -f "$workflow" || {
    echo "not ok 1 - experimental jobs run every registered test"
    echo "# missing experimental workflow"
    exit 1
}

awk '
function fail(message) {
    print "# " message
    failed=1
}

function finish_job() {
    if (target == "") {
        return
    }

    seen[target]++
    if (forbidden != 0) {
        fail(target " disables tests or selects a smoke-test subset")
    }

    if (target == "multiarch-linux") {
        if (labels != 8) {
            fail(target " must retain eight architecture/build-system jobs")
        }
        if (make_build != 1 || make_check != 1) {
            fail(target " must build all Autotools targets and run make check")
        }
        if (meson_build != 1 || meson_test != 1) {
            fail(target " must build all Meson targets and run meson test")
        }
    } else if (target == "sparc64-netbsd") {
        if (labels != 2) {
            fail(target " must retain both build-system jobs")
        }
        if (gmake_build != 1 || gmake_check != 1) {
            fail(target " must build all Autotools targets and run gmake check")
        }
        if (meson_build != 1 || meson_test != 1) {
            fail(target " must build all Meson targets and run meson test")
        }
    } else if (target == "icx-x86_64") {
        if (labels != 2) {
            fail(target " must retain Linux and Windows jobs")
        }
        if (meson_build != 2 || meson_test != 2) {
            fail(target " must build all targets and run both Meson suites")
        }
        if (windows_sh != 1 || windows_cp != 1) {
            fail(target " must expose the POSIX test tools on Windows")
        }
    }

    target=""
}

function start_job(name) {
    target=name
    labels=0
    forbidden=0
    make_build=0
    make_check=0
    gmake_build=0
    gmake_check=0
    meson_build=0
    meson_test=0
    windows_sh=0
    windows_cp=0
}

/^  [a-z0-9][a-z0-9_-]*:/ {
    finish_job()
    name=$0
    sub(/^  /, "", name)
    sub(/:.*$/, "", name)
    if (name == "multiarch-linux" ||
        name == "sparc64-netbsd" || name == "icx-x86_64") {
        start_job(name)
    }
    next
}

target != "" {
    if ($0 ~ /^          - label:/) {
        labels++
    }
    if (index($0, "--disable-tests") != 0 ||
        index($0, "-Dtests=false") != 0 ||
        index($0, "smoke_test") != 0 ||
        $0 ~ /^[[:space:]]+set --/) {
        forbidden++
    }
    if ($0 ~ /^[[:space:]]+make -j2 V=1$/) {
        make_build++
    }
    if ($0 ~ /^[[:space:]]+make -j2 check$/) {
        make_check++
    }
    if ($0 ~ /^[[:space:]]+gmake -j2 V=1$/) {
        gmake_build++
    }
    if ($0 ~ /^[[:space:]]+gmake -j2 check$/) {
        gmake_check++
    }
    if ($0 ~ /^[[:space:]]+meson compile -C \/tmp\/libsixel-build/) {
        meson_build++
    }
    if ($0 ~ /^[[:space:]]+meson compile -C builddir-icx -v$/) {
        meson_build++
    }
    if ($0 ~ /^[[:space:]]+meson test -C \/tmp\/libsixel-build/) {
        meson_test++
    }
    if ($0 ~ /^[[:space:]]+meson test -C builddir-icx/) {
        meson_test++
    }
    if ($0 ~ /^[[:space:]]+where sh$/) {
        windows_sh++
    }
    if ($0 ~ /^[[:space:]]+where cp$/) {
        windows_cp++
    }
}

END {
    finish_job()
    if (seen["multiarch-linux"] != 1 ||
        seen["sparc64-netbsd"] != 1 || seen["icx-x86_64"] != 1) {
        fail("required experimental job family is missing or duplicated")
    }
    exit failed
}
' "$workflow" || {
    echo "not ok 1 - experimental jobs run every registered test"
    exit 1
}

echo "ok 1 - experimental jobs run every registered test"
exit 0
