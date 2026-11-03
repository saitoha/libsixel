#!/bin/sh
# Keep Emscripten CI on one executable and readable coverage format.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
awk '
function valid(text) {
    return text ~ /(--enable-gcov|-Db_coverage=true)/ &&
        text ~ /gcovr/ &&
        text !~ /(-fprofile-instr-generate|-fcoverage-mapping|LLVM_PROFILE_FILE)/
}
function finish() {
    if (selected) {
        ++checked
        if (!valid(block)) {
            print "# " label ": use gcov coverage without source-profile flags"
            failed = 1
        }
    }
}
BEGIN {
    if (!valid("--enable-gcov gcovr") || !valid("-Db_coverage=true gcovr") ||
        valid("-Db_coverage=true gcovr -fprofile-instr-generate") ||
        valid("-Db_coverage=true") || valid("-O0 -g gcovr")) {
        failed = 1
        exit 1
    }
}
/^[[:space:]]*- label:/ {
    finish()
    label = $0
    selected = label ~ /emscripten-coverage/
    block = ""
}
{ if (selected) block = block "\n" $0 }
END {
    finish()
    exit failed || checked == 0
}
' "$src_root/.github/workflows/ci.yml" >&2 || {
    echo 'not ok 1 - Emscripten CI uses one coverage format'
    exit 0
}
echo 'ok 1 - Emscripten CI uses one coverage format'
exit 0
