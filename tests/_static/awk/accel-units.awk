# List acceleration units and the Accel tags written inside them.
#
# A unit is one top-level construct of a C, Objective-C or Metal source:
# a function, a declaration, or a #define with its continuation lines.
# Comments and preprocessor lines before a construct belong to it, so a tag
# written directly above a function is part of that function's unit. A
# GLSL file is one unit as a whole.
#
# A unit is reported when it uses SIMD vector types or intrinsics, declares
# a Metal kernel, or embeds Metal kernel source in a string.
# Output, one tab-separated record per unit:
#   path  line  name  families  tags
# families is a comma list of neon, sse2, avx2, wasm, metal and vulkan;
# tags is a space list of ID/backend pairs. A malformed tag is reported as
#   !malformed  path  line

function strip(s,   out, c, i, n) {
    # Most lines hold no comment, string or character literal.
    if (!incomment && s !~ /[\/"']/)
        return s
    out = ""
    n = length(s)
    for (i = 1; i <= n; i++) {
        c = substr(s, i, 1)
        if (incomment) {
            if (c == "*" && substr(s, i + 1, 1) == "/") {
                incomment = 0
                i++
            }
            continue
        }
        if (instring) {
            if (c == "\\") {
                i++
                continue
            }
            if (c == quote)
                instring = 0
            continue
        }
        if (c == "/" && substr(s, i + 1, 1) == "*") {
            incomment = 1
            i++
            continue
        }
        if (c == "/" && substr(s, i + 1, 1) == "/")
            break
        if (c == "\"" || c == "'") {
            instring = 1
            quote = c
            out = out " "
            continue
        }
        out = out c
    }
    return out
}

function add_family(f) {
    if (f != "" && index("," families ",", "," f ",") == 0)
        families = families (families == "" ? "" : ",") f
}

function scan_code(s) {
    if (s ~ /_mm256_[a-z]|__m256/)
        add_family("avx2")
    if (s ~ /_mm_[a-z]|__m128/)
        add_family("sse2")
    if (s ~ /wasm_[a-z0-9]+_[a-z]|v128_t/)
        add_family("wasm")
    if (s ~ /(u?int|float|poly)(8|16|32|64)x[0-9]+(x[0-9])?_t/ ||
        s ~ /(^|[^A-Za-z0-9_])v[a-z0-9]+_(n_|lane_|laneq_|high_)?[supf](8|16|32|64)[ \t]*\(/)
        add_family("neon")
    if (is_metal && s ~ /(^|[^A-Za-z0-9_])kernel[ \t]+void|\[\[[ \t]*kernel[ \t]*\]\]/)
        add_family("metal")
}

function scan_raw(s) {
    if (!is_metal && s ~ /"[^"]*kernel[ \t]+void/)
        add_family("metal")
}

function scan_tags(s,   rest, token, id, count, i, parts) {
    rest = s
    while (match(rest, /Accel:/)) {
        rest = substr(rest, RSTART + RLENGTH)
        token = rest
        sub(/\*\/.*/, "", token)
        sub(/^[ \t]+/, "", token)
        sub(/[ \t]+$/, "", token)
        count = split(token, parts, /[ \t]+/)
        id = parts[1]
        if (count < 2 || id !~ /^[A-Z][A-Z0-9]*(-[A-Z0-9]+)*$/) {
            print "!malformed\t" path "\t" FNR
            continue
        }
        for (i = 2; i <= count; i++) {
            if (parts[i] !~ /^(scalar|neon|sse2|avx2|wasm|metal|vulkan)$/) {
                print "!malformed\t" path "\t" FNR
                break
            }
            if (index(" " tags " ", " " id "/" parts[i] " ") == 0)
                tags = tags (tags == "" ? "" : " ") id "/" parts[i]
        }
    }
}

function pick_name(s,   rest, word, depth0, c, i, n, best, last) {
    # First identifier followed by "(" outside parentheses, skipping
    # attributes and all-capital macros when a better name follows.
    n = length(s)
    depth0 = 0
    best = ""
    word = ""
    for (i = 1; i <= n; i++) {
        c = substr(s, i, 1)
        if (c ~ /[A-Za-z0-9_]/) {
            word = word c
            continue
        }
        if (c == " " || c == "\t") {
            if (word != "")
                last = word
            word = ""
            continue
        }
        if (word == "" && c == "(" && last != "")
            word = last
        if (c == "(" && depth0 == 0 && word ~ /^[A-Za-z_]/ &&
            word !~ /^(__attribute__|__declspec|sizeof|defined|alignas|_Alignas|__asm__|__asm)$/) {
            if (word !~ /^[A-Z0-9_]+$/)
                return word
            if (best == "")
                best = word
        }
        if (c == "(")
            depth0++
        else if (c == ")")
            depth0--
        last = word != "" ? word : last
        word = ""
    }
    return best
}

function fallback_name(s,   head) {
    head = s
    sub(/[\[=;{].*/, "", head)
    sub(/[ \t]+$/, "", head)
    if (match(head, /[A-Za-z_][A-Za-z0-9_]*$/))
        return substr(head, RSTART, RLENGTH)
    return ""
}

function flush() {
    if (families != "") {
        if (name == "")
            name = fallback_name(first_code)
        print path "\t" start "\t" name "\t" families "\t" tags
    }
    families = ""
    tags = ""
    name = ""
    first_code = ""
    start = FNR + 1
}

function begin_file() {
    path = FILENAME
    sub(/^\.\//, "", path)
    depth = 0
    incomment = 0
    instring = 0
    in_define = 0
    families = ""
    tags = ""
    name = ""
    first_code = ""
    start = 1
    is_metal = FILENAME ~ /\.metal$/
    is_glsl = FILENAME ~ /\.glsl$/
    if (is_glsl) {
        families = "vulkan"
        name = path
        sub(/.*\//, "", name)
    }
}

FNR == 1 {
    if (NR > 1) {
        if (was_glsl)
            print last_path "\t1\t" glsl_name "\tvulkan\t" glsl_tags
        else
            flush()
    }
    begin_file()
    was_glsl = is_glsl
}

is_glsl {
    scan_tags($0)
    glsl_tags = tags
    glsl_name = name
    last_path = path
    next
}

{
    raw = $0
    scan_tags(raw)
    if (in_define) {
        scan_code(strip(raw))
        scan_raw(raw)
        if (raw !~ /\\[ \t]*$/) {
            in_define = 0
            flush()
        }
        next
    }
    if (depth == 0 && raw ~ /^[ \t]*#[ \t]*define[ \t]/) {
        if (name == "") {
            name = raw
            sub(/^[ \t]*#[ \t]*define[ \t]+/, "", name)
            sub(/[^A-Za-z0-9_].*/, "", name)
        }
        scan_code(strip(raw))
        scan_raw(raw)
        if (raw ~ /\\[ \t]*$/)
            in_define = 1
        else
            flush()
        next
    }
    if (raw ~ /^[ \t]*#/ && !incomment)
        next
    scan_raw(raw)
    line = strip(raw)
    if (depth == 0 && line ~ /[^ \t]/) {
        if (first_code == "")
            first_code = line
        if (name == "")
            name = pick_name(line)
    }
    scan_code(line)
    # A construct ends when a line returns the brace depth to zero, or ends
    # a declaration at depth zero.
    before = depth
    depth += gsub(/\{/, "{", line) - gsub(/\}/, "}", line)
    if (depth == 0 && (before > 0 || line ~ /[;}]/))
        flush()
}

END {
    if (NR > 0) {
        if (was_glsl)
            print last_path "\t1\t" glsl_name "\tvulkan\t" glsl_tags
        else
            flush()
    }
}
