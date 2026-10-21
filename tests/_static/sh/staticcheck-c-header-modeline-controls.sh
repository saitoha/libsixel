#!/bin/sh
# Verify bounded header/tail windows and Git worktree metadata handling.
# Policy: docs/testing/staticcheck.md
set -eux

echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-modeline-controls-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir -p "$tmpdir/root/src" "$tmpdir/bin"
# A Git-file worktree must take the same inventory path as a .git directory.
printf '%s\n' 'gitdir: control' > "$tmpdir/root/.git"
cat > "$tmpdir/bin/git" <<'GIT'
#!/bin/sh
printf '%s\n' src/valid.c src/tail-outside.h src/header-outside.c src/other.c src/empty.h src/deleted.c
GIT
chmod +x "$tmpdir/bin/git"
awk -v root="$tmpdir/root/src" '
BEGIN {
    for (i=1; i<=200; i++) {
        line="padding"
        if (i==120) line="SPDX-License-Identifier: MIT"
        if (i==160) line="Permission is hereby granted Copyright (c) 2026"
        if (i==161) line="emacs Local Variables vim: set"
        print line > (root "/valid.c")
        if (i==160) line=line " emacs Local Variables vim: set"
        if (i==161) line="padding"
        print line > (root "/tail-outside.h")
        line="padding"
        if (i==120) line="SPDX-License-Identifier: MIT"
        if (i==161) line="Permission is hereby granted Copyright (c) 2026"
        if (i==200) line="emacs Local Variables vim: set"
        print line > (root "/header-outside.c")
    }
    print "SPDX-License-Identifier: BSD-2-Clause" > (root "/other.c")
    printf "" > (root "/empty.h")
}'
status=0
PATH="$tmpdir/bin:$PATH" sh \
    "$src_root/tests/_static/sh/staticcheck-c-header-modeline.sh" \
    "$tmpdir/root" > "$tmpdir/actual" || status=$?
test "$status" -eq 1 || {
    echo 'not ok 1 - invalid modelines must fail in a Git-file worktree'
    exit 0
}
printf '%s\n' '1..1' \
    '# src/tail-outside.h: missing emacs modeline near file end' \
    '# src/tail-outside.h: missing vim modeline near file end' \
    '# src/header-outside.c: missing MIT license grant' \
    '# src/header-outside.c: missing copyright header' \
    'not ok 1 - new C/C header files include header comment and modeline' \
    > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/actual" || {
    echo 'not ok 1 - header and tail window boundaries or diagnostics changed'
    exit 0
}
printf '#!/bin/sh\nexit 2\n' > "$tmpdir/bin/git"
status=0
PATH="$tmpdir/bin:$PATH" sh \
    "$src_root/tests/_static/sh/staticcheck-c-header-modeline.sh" \
    "$tmpdir/root" > "$tmpdir/actual" || status=$?
test "$status" -eq 1 || {
    echo 'not ok 1 - inventory failure must not become a successful skip'
    exit 0
}
printf '%s\n' '1..1' \
    'not ok 1 - failed to inventory tracked C/C header files' \
    > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/actual" || {
    echo 'not ok 1 - inventory failure lost its diagnostic'
    exit 0
}
echo 'ok 1 - modeline scanning preserves windows and checks Git worktrees'
