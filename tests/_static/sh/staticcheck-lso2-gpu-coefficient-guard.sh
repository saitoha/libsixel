#!/bin/sh
# Hoisted GPU-only tables must remain absent from ordinary C builds.
# Policy: docs/build/autotools.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(text) {
    return text ~ /^#if defined\(HAVE_METAL\) \|\| defined\(HAVE_VULKAN\)\n/ &&
        text ~ /\n#endif \/\* HAVE_METAL \|\| HAVE_VULKAN \*\/\n$/ &&
        text !~ /\n#[ \t]*(if|else|elif|endif)[^\n]*\n[^\n]/
}
function generator_valid(text,    begin, end, table) {
    begin = "printf \047%s\\n\047 \047#if defined(HAVE_METAL) || " \
        "defined(HAVE_VULKAN)\047\n"
    end = "printf \047%s\\n\047 \047#endif /* HAVE_METAL || " \
        "HAVE_VULKAN */\047\n"
    table = "\"$AWK\" -v NAME=lso2_gpu_ratios"
    return index(text, begin) && index(text, table) > index(text, begin) &&
        substr(text, length(text) - length(end) + 1) == end
}
FNR == 1 { name = FILENAME }
{ content[name] = content[name] $0 "\n" }
END {
    header = content[ARGV[1]]
    script = content[ARGV[2]]
    # Inspect the emitted guard without regenerating thousands of ratios.
    changed_script = script
    sub(/HAVE_METAL/, "HAVE_THREADS", changed_script)
    removed = header
    sub(/^#if[^\n]*\n/, "", removed)
    premature = "#if defined(HAVE_METAL) || defined(HAVE_VULKAN)\n" \
        "#endif\nint table[2];\n#endif /* HAVE_METAL || HAVE_VULKAN */\n"
    exit !valid(header) || valid(removed) || valid(premature) ||
        valid("") || !generator_valid(script) || generator_valid(changed_script)
}
' "$1/src/lso2-gpu-coefficients.h" "$1/tools/gen-lso2-gpu.sh" || {
    echo 'not ok 1 - GPU LSO2 tables retain their GPU feature guard'
    exit 0
}
echo 'ok 1 - GPU LSO2 tables retain their GPU feature guard'
exit 0
