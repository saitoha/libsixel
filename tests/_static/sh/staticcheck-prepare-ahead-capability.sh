#!/bin/sh
# Helper-lane tests must use the production compile-time capability.
# Policy: docs/threading/encoder.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function clean(text) {
    gsub(/\/\*([^*]|\*+[^*\/])*\*+\//, "", text)
    return text
}
function guarded(text) {
    text = clean(text)
    return index(text, "#if SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED\n") &&
        !index(text, "#if SIXEL_ENABLE_THREADS")
}
function test_body(text) {
    return guarded(text) && index(text, "#else\n") &&
        index(text, "return 77;")
}
function needs_helper(text) {
    text = clean(text)
    gsub(/[[:space:]]/, "", text)
    return text ~ /gpu_ahead_wait\([^)]*[.]helper_(complete|entered)\)/ ||
        index(text, "gpu_ahead_compare(")
}
function finish() {
    if (needs_helper(source) && index(source, "#include \"tests/_support/planner/pipeline/gpu_ahead_common.h\"")) {
        ++tests
        if (!test_body(source)) {
            print "# missing prepare-ahead capability guard: " file
            failed = 1
        }
    }
}
BEGIN {
    good = "#if SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED\nwork();\n" \
        "#else\nreturn 77;\n#endif\n"
    threads = good
    sub(/SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED/, "SIXEL_ENABLE_THREADS", threads)
    no_skip = good
    sub(/return 77;/, "return 0;", no_skip)
    commented = "/* " good " */\nwork();"
    if (!test_body(good) || test_body(threads) || test_body(no_skip) ||
            test_body(commented) || !needs_helper("gpu_ahead_compare(x)") ||
            !needs_helper("gpu_ahead_wait(&s, &s.helper_entered)") ||
            needs_helper("gpu_ahead_wait(&s, &s.writer_entered)") ||
            needs_helper("/* gpu_ahead_compare(x) */")) failed = 1
}
FNR == 1 { finish(); source = ""; file = FILENAME }
FILENAME == ARGV[1] { header = header $0 "\n"; next }
FILENAME == ARGV[2] {
    if ($0 ~ /^sixel_encoder_pipeline_prepare_ahead_eligible\(/) active = 1
    if (active) production = production $0 "\n"
    if ($0 == "}") active = 0
    next
}
{ source = source $0 "\n" }
END {
    finish()
    header = clean(header)
    expected = "#if SIXEL_ENABLE_THREADS && !defined(__PCC__) && " \
        "!defined(__TINYC__)\n#define SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED 1\n" \
        "#else\n#define SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED 0\n#endif"
    if (!index(header, expected) ||
            !index(clean(production), "#if !SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED\n") ||
            tests == 0) failed = 1
    exit failed
}
' "$1/src/encoder.h" "$1/src/encoder.c" \
    "$1"/tests/encoder/planning/*.c || {
    echo 'not ok 1 - helper-lane tests share production capability'
    exit 0
}
echo 'ok 1 - helper-lane tests share production capability'
exit 0
