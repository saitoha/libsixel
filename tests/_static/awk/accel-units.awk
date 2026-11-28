# List acceleration units and the Accel tags written inside them.
#
# A unit is one top-level construct of a C, Objective-C or Metal source:
# a function, a declaration, or a #define with its continuation lines.
# Comments and preprocessor lines before a construct belong to it, so a tag
# written directly above a function is part of that function's unit. A
# GLSL or WGSL file, or a Metal fragment without kernels, is one unit.
#
# A unit is reported when it uses SIMD vector types or intrinsics, declares
# a Metal kernel, embeds a compute shader in a string, or is a shader
# source file.
# Output, one tab-separated record per unit:
#   path  line  name  families  tags
# families is a comma list of detected backends, with "unsupported" for
# vector constructs outside the ledger. tags is a space list of ID/backend
# pairs. Invalid tags are reported as !malformed or !misplaced, path, line.

function strip(s,   out, c, i, n, tail, end) {
    # Most lines hold no comment, string or character literal.
    if (!incomment && !instring && s !~ /[\/"']/) {
        if (s ~ /[^ \t]/)
            shader_literal = ""
        return s
    }
    out = ""
    n = length(s)
    for (i = 1; i <= n; i++) {
        c = substr(s, i, 1)
        if (incomment) {
            tail = substr(s, i)
            end = index(tail, "*/")
            scan_comment(end ? substr(tail, 1, end - 1) : tail, out)
            if (!end)
                break
            incomment = 0
            i += end
            out = out " "
            continue
        }
        if (instring) {
            if (c == "\\") {
                c = substr(s, i + 1, 1)
                if (quote == "\"" && c ~ /^[nrt]$/)
                    shader_literal = shader_literal " "
                i++
                continue
            }
            if (c == quote) {
                instring = 0
                if (quote == "\"" && !is_metal && !is_glsl && !is_wgsl)
                    scan_literal()
            } else if (quote == "\"")
                shader_literal = shader_literal c
            continue
        }
        if (c == "/" && substr(s, i + 1, 1) == "*") {
            incomment = 1
            i++
            continue
        }
        if (c == "/" && substr(s, i + 1, 1) == "/") {
            scan_comment(substr(s, i + 2), out)
            break
        }
        if (c == "\"" || c == "'") {
            instring = 1
            quote = c
            out = out " "
            continue
        }
        if (c !~ /[ \t]/)
            shader_literal = ""
        out = out c
    }
    return out
}

function add_family(f) {
    if (f != "" && index("," families ",", "," f ",") == 0)
        families = families (families == "" ? "" : ",") f
}

function scan_code(s) {
    # Most code in a vector-capable file is shared with the scalar path.
    if (s !~ /_mm|__m|wasm_|v128_t|[0-9]x[0-9]|v[a-z0-9_]+_[supfb][0-9]|__builtin_|vector_type|vector_size|kernel/)
        return
    if (s ~ /(^|[^a-zA-Z0-9_])(_mm256_[a-zA-Z0-9_]+|__m256[idh]?)([^a-zA-Z0-9_]|$)/)
        add_family("avx2")
    if (s ~ /(^|[^a-zA-Z0-9_])(_mm_[a-zA-Z0-9_]+|__m128[idh]?)([^a-zA-Z0-9_]|$)/)
        add_family("sse2")
    if (s ~ /(^|[^a-zA-Z0-9_])(wasm_(v128|[iuf][0-9]+x[0-9]+)_[a-zA-Z0-9_]+|v128_t)([^a-zA-Z0-9_]|$)/)
        add_family("wasm")
    if (s ~ /(^|[^A-Za-z0-9_])(u?int|float|bfloat|mfloat|poly)(8|16|32|64)x[0-9]+(x[0-9]+)?_t([^A-Za-z0-9_]|$)/ ||
        s ~ /(^|[^A-Za-z0-9_])v[a-z0-9_]+_[supfb](8|16|32|64)[ \t]*\(/ ||
        ((in_define || s ~ /^[ \t]*#/) &&
         s ~ /(^|[^A-Za-z0-9_])v[a-z0-9_]+_[supfb](8|16|32|64)([^A-Za-z0-9_]|$)/))
        add_family("neon")
    if (s ~ /(^|[^A-Za-z0-9_])(_mm512_[A-Za-z0-9_]+|__m(64|512)[idh]?|(__)?(vector_size|ext_vector_type)(__)?|__builtin_(shufflevector|convertvector))([^A-Za-z0-9_]|$)/ ||
        s ~ /(^|[^A-Za-z0-9_])__builtin_(ia32|neon|wasm)_[A-Za-z0-9_]/)
        add_family("unsupported")
    if (is_metal && s ~ /(^|[^A-Za-z0-9_])kernel([^A-Za-z0-9_]|$)/)
        add_family("metal")
}

function scan_literal() {
    if (shader_literal ~ /(^|[^A-Za-z0-9_])kernel[ \t]+void[ \t]+[A-Za-z_]|\[\[[ \t]*kernel[ \t]*\]\]/)
        add_family("metal")
    if (shader_literal ~ /@compute([^A-Za-z0-9_]|$)/)
        add_family("webgpu")
    if (shader_literal ~ /gl_GlobalInvocationID|layout[ \t]*\([ \t]*local_size_x/)
        add_family("vulkan")
    # Preserve enough of the tail to match a keyword crossing literal lines.
    if (length(shader_literal) > 80)
        shader_literal = substr(shader_literal, length(shader_literal) - 79)
}

function scan_comment(s, code,   id, count, i, parts, pair) {
    if (!sub(/^[ \t]*(\*[ \t]*)?Accel:[ \t]*/, "", s))
        return
    if (!is_glsl && !is_wgsl &&
        (depth != 0 || first_code != "" || in_define || code ~ /[^ \t]/)) {
        print "!misplaced\t" path "\t" FNR
        return
    }
    sub(/[ \t\r]+$/, "", s)
    count = split(s, parts, /[ \t]+/)
    id = parts[1]
    if (count < 2 || id !~ /^[A-Z][A-Z0-9]*(-[A-Z0-9]+)*$/) {
        print "!malformed\t" path "\t" FNR
        return
    }
    for (i = 2; i <= count; i++) {
        pair = id "/" parts[i]
        if ((parts[i] !~ /^(scalar|neon|sse2|avx2|wasm)$/ &&
             parts[i] !~ /^(metal|vulkan|webgpu)(-fp32)?$/) ||
            index(" " tags " ", " " pair " ")) {
            print "!malformed\t" path "\t" FNR
            return
        }
        tags = tags (tags == "" ? "" : " ") pair
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

function flush(   record) {
    if (families != "" || tags != "") {
        if (name == "")
            name = fallback_name(first_code)
        record = path "\t" start "\t" name "\t" families "\t" tags
        if (is_metal) {
            metal_records[++metal_count] = record
            if (index("," families ",", ",metal,"))
                metal_kernel = 1
        } else
            print record
    }
    families = ""
    tags = ""
    name = ""
    first_code = ""
    shader_literal = ""
    start = FNR + 1
}

function begin_file() {
    path = FILENAME
    sub(/^\.\//, "", path)
    depth = 0
    incomment = 0
    instring = 0
    in_define = 0
    shader_literal = ""
    families = ""
    tags = ""
    name = ""
    first_code = ""
    start = 1
    is_metal = FILENAME ~ /\.metal$/
    is_glsl = FILENAME ~ /\.(glsl|comp)$/
    is_wgsl = FILENAME ~ /\.wgsl$/
    metal_count = 0
    metal_kernel = 0
    header_seen = 0
    header_tags = ""
}

function end_file(   filename, i, family) {
    filename = path
    sub(/.*\//, "", filename)
    if (is_glsl || is_wgsl) {
        family = is_glsl ? "vulkan" : "webgpu"
        print path "\t1\t" filename "\t" family "\t" tags
        return
    }
    flush()
    if (!is_metal)
        return
    if (!metal_kernel) {
        print path "\t1\t" filename "\tmetal\t" header_tags
        # The first record, if present, may be the fragment's header tag.
        if (header_tags != "" && metal_count &&
            metal_records[1] ~ ("\t\t" header_tags "$"))
            i = 2
        else
            i = 1
    } else
        i = 1
    for (; i <= metal_count; i++)
        print metal_records[i]
}

FNR == 1 {
    if (NR > 1)
        end_file()
    begin_file()
}

is_glsl || is_wgsl {
    strip($0)
    next
}

{
    raw = $0
    line = strip(raw)
    if (is_metal && !header_seen && line ~ /[^ \t]/) {
        header_seen = 1
        header_tags = tags
    }
    if (in_define) {
        scan_code(line)
        if (raw !~ /\\[ \t]*$/) {
            in_define = 0
            if (depth == 0)
                flush()
        }
        next
    }
    if (line ~ /^[ \t]*#[ \t]*define[ \t]/) {
        if (depth == 0 && name == "") {
            name = line
            sub(/^[ \t]*#[ \t]*define[ \t]+/, "", name)
            sub(/[^A-Za-z0-9_].*/, "", name)
        }
        scan_code(line)
        if (raw ~ /\\[ \t]*$/)
            in_define = 1
        else if (depth == 0)
            flush()
        next
    }
    if (line ~ /^[ \t]*#/)
        next
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
    if (NR > 0)
        end_file()
}
