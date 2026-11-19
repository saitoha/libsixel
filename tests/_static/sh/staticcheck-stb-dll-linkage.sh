#!/bin/sh
# Preserve one DLL declaration for stb functions shared across source files.
set -eux
echo '1..1'
set -v

awk '
function inspect(path, text,    name, declaration, owner, count, pattern) {
    if (path ~ /\/stb_image\.h$/) return
    owner = (path ~ /\/loader-builtin\.h$/)
    gsub(/\\\r?\n/, "", text)
    if (!owner && !index(text, "stbi_")) return
    gsub(/\/\*([^*]|\*+[^*\/])*\*+\//, " ", text)
    gsub(/\/\/[^\n]*/, " ", text)
    gsub(/[[:space:]]+/, " ", text)
    for (name in types) {
        pattern = "(^|[^A-Za-z0-9_])" types[name] name " *\\("
        declaration = "SIXEL_INTERNAL_API " types[name] name " *\\("
        count = gsub(pattern, "&", text)
        if ((owner && (count != 1 || text !~ declaration)) ||
                (!owner && count)) {
            print "# " path ": use the DLL declaration for " name \
                " in loader-builtin.h"
            failed = 1
        }
    }
}
BEGIN {
    # Read one whole C file per record, including multiline declarations.
    RS = "\034"
    types["stbi_image_free"] = "void +"
    types["stbi_zlib_decode_malloc_guesssize_headerflag"] = "char *[*] *"
}
{ inspect(FILENAME, $0) }
END { exit failed }
' "$1"/src/*.c "$1"/src/*.h || {
    echo 'not ok 1 - shared stb functions use one DLL declaration'
    exit 0
}
echo 'ok 1 - shared stb functions use one DLL declaration'
exit 0
