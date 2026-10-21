#!/bin/sh
# Emit TAP for header-comment/modeline checks on tracked C/C header files.

set -eu

echo "1..1"

src_root=$1

cd "$src_root"

if { test ! -d .git && test ! -f .git; } ||
        ! command -v git >/dev/null 2>&1; then
    echo "ok 1 # SKIP git metadata not available"
    exit 0
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-c-header-modeline-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

git -c core.quotepath=false ls-files -- 'src/*.c' 'src/*.h' > "$tmpfile" || {
    echo "not ok 1 - failed to inventory tracked C/C header files"
    exit 1
}

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no tracked C/C header files"
    exit 0
fi

# Filter deleted tracked files with a shell builtin. One awk process reads
# every remaining file once; no per-file sed/grep/tail pipeline is needed.
while IFS= read -r path; do
    test -n "$path" || continue
    test -f "$path" || continue
    printf '%s\n' "$path"
done < "$tmpfile" | awk '
{
    path = $0
    checked++
    number = 0
    seen_spdx = 0
    spdx = ""
    license = grant = copyright = emacs = vim = 0
    while ((status = (getline line < path)) > 0) {
        number++
        if (number <= 120) {
            if (!seen_spdx && index(line, "SPDX-License-Identifier:")) {
                seen_spdx = 1
                spdx = line
                sub(/.*SPDX-License-Identifier:[[:space:]]*/, "", spdx)
            }
            if (index(line, "SPDX-License-Identifier: MIT")) license = 1
        }
        if (number <= 160) {
            if (index(line, "Permission is hereby granted")) grant = 1
            if (index(line, "Copyright (c) ")) copyright = 1
        }
        # Only the last matching line matters for the final 40-line window.
        if (index(line, "emacs Local Variables")) emacs = number
        if (index(line, "vim: set")) vim = number
    }
    close(path)
    if (status < 0) {
        print "# " path ": unable to read tracked source"
        failed = 1
        next
    }
    if (spdx != "MIT") next
    if (!license) {
        print "# " path ": missing SPDX license header"
        failed = 1
    }
    if (!grant) {
        print "# " path ": missing MIT license grant"
        failed = 1
    }
    if (!copyright) {
        print "# " path ": missing copyright header"
        failed = 1
    }
    if (!emacs || emacs <= number - 40) {
        print "# " path ": missing emacs modeline near file end"
        failed = 1
    }
    if (!vim || vim <= number - 40) {
        print "# " path ": missing vim modeline near file end"
        failed = 1
    }
}
END {
    if (!checked) {
        print "ok 1 # SKIP no MIT-licensed C/C header files"
    } else if (failed) {
        print "not ok 1 - new C/C header files include header comment and modeline"
    } else {
        print "ok 1 - new C/C header files include header comment and modeline"
    }
    exit failed ? 1 : 0
}'
