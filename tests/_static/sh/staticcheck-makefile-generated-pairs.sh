#!/bin/sh
# Emit TAP for the staged Makefile.am/Makefile.in pair guard.

set -eux

src_root=$1
checker=$src_root/tools/check_makefile_generated_pairs.sh

echo "1..1"
set -v

"$checker" --simulate 'Makefile.in' '' >/dev/null 2>&1 || {
    echo "not ok 1 - generated-only repair commits remain valid"
    exit 0
}
"$checker" --simulate 'Makefile.am
Makefile.in' '' >/dev/null 2>&1 || {
    echo "not ok 1 - a fully staged root pair is accepted"
    exit 0
}
"$checker" --simulate 'tests/Makefile.am
tests/Makefile.in' '' >/dev/null 2>&1 || {
    echo "not ok 1 - a fully staged subdirectory pair is accepted"
    exit 0
}
"$checker" --simulate 'Makefile.am' '' >/dev/null 2>&1 && {
    echo "not ok 1 - a missing generated file is rejected"
    exit 0
}
"$checker" --simulate 'Makefile.am
Makefile.in' 'Makefile.in' >/dev/null 2>&1 && {
    echo "not ok 1 - an unstaged generated hunk is rejected"
    exit 0
}
"$checker" --simulate 'Makefile.am
Makefile.in' 'Makefile.am' >/dev/null 2>&1 && {
    echo "not ok 1 - an unstaged source hunk is rejected"
    exit 0
}

echo "ok 1 - staged Makefile source/generated pairs are complete"
exit 0
