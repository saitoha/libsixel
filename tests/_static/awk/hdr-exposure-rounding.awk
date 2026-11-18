# Protect the explicit double rounding boundary before HDR range validation.
# This is a source contract for loader_hdr_finish, not general C data flow.
function clean(text,    out, token) {
    out = ""
    gsub(/\\\r?\n/, "", text)
    while (match(text, /"([^"\\]|\\.)*"|'([^'\\]|\\.)*'|\/\*|\/\//)) {
        token = substr(text, RSTART, RLENGTH)
        out = out substr(text, 1, RSTART - 1) " "
        text = substr(text, RSTART + RLENGTH)
        if (token == "/*") {
            if (!match(text, /\*\//)) return out
            text = substr(text, RSTART + RLENGTH)
        } else if (token == "//") {
            if (!index(text, "\n")) return out
            text = substr(text, index(text, "\n") + 1)
        } else out = out " literal "
    }
    return out text
}
function body_of(text,    i, begin, depth, c) {
    if (!match(text, /loader_hdr_finish[[:space:]]*\(/)) return ""
    text = substr(text, RSTART + RLENGTH)
    begin = index(text, "{")
    if (!begin) return ""
    depth = 1
    for (i = begin + 1; i <= length(text); ++i) {
        c = substr(text, i, 1)
        if (c == "{") ++depth
        if (c == "}" && --depth == 0) {
            return substr(text, begin + 1, i - begin - 1)
        }
    }
    return ""
}
function guarded(body, name,    code, start, prefix, tail, end, result, stmt,
        condition, after, calls, scratch) {
    code = body
    gsub(/[[:space:]]/, "", code)
    if (!match(code, "(^|[^A-Za-z0-9_])" name "=pow\\(")) return 0
    start = RSTART + (substr(code, RSTART, 1) == substr(name, 1, 1) ? 0 : 1)
    prefix = substr(code, 1, start - 1)
    # The rounding store must execute unconditionally before any pixel use.
    if (prefix ~ /[{}?:#]|pixels|return|goto|(^|[^A-Za-z0-9_])(if|for|while|switch)\(/) return 0
    tail = substr(code, start)
    if (!match(tail, "^" name "=pow\\([^;{}]*\\);")) return 0
    tail = substr(tail, RLENGTH + 1)
    result = name
    # Permit either direct validation of the volatile object or one read into
    # an ordinary local, so the pixel loop need not keep loading volatile data.
    end = index(tail, ";")
    stmt = substr(tail, 1, end)
    if (stmt ~ /^[A-Za-z_][A-Za-z0-9_]*=/ &&
            substr(stmt, index(stmt, "=") + 1) == name ";") {
        result = substr(stmt, 1, index(stmt, "=") - 1)
        tail = substr(tail, end + 1)
    }
    if (index(tail, "if(!isfinite(" result ")||") != 1) return 0
    end = index(tail, "){return0;}")
    if (!end) return 0
    condition = substr(tail, 1, end - 1)
    if (condition ~ /[?:;{},]/) return 0
    after = substr(tail, end + length("){return0;}"))
    if (!index(after, "pixels[")) return 0
    # Do not allow a second libm evaluation or overwrite to bypass the checked
    # value after the early rejection. Keep future arithmetic changes reviewed.
    scratch = code
    calls = gsub(/pow\(/, "", scratch)
    if (calls != 1 || after ~ ("(^|[^A-Za-z0-9_])" result \
            "(=[^=]|[-+*/]=|[+][+]|--)")) return 0
    return 1
}
function valid(text,    body, rest, decl, fields, n, name) {
    body = body_of(clean(text))
    rest = body
    while (match(rest, /(volatile[[:space:]]+double|double[[:space:]]+volatile)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*;/)) {
        decl = substr(rest, RSTART, RLENGTH)
        rest = substr(rest, RSTART + RLENGTH)
        sub(/[[:space:]]*;$/, "", decl)
        n = split(decl, fields, /[[:space:]]+/)
        name = fields[n]
        if (guarded(body, name)) return 1
    }
    return 0
}
function rejected(text, label) {
    if (valid(text)) {
        print "HDR rounding control accepted invalid " label
        failed = 1
    }
}
BEGIN {
    good = "int loader_hdr_finish(float *pixels, size_t count) { " \
        "volatile double rounded; double exposure; " \
        "rounded = pow(2.0, exposure); exposure = rounded; " \
        "if (!isfinite(exposure) || exposure <= 0.0) { return 0; } " \
        "pixels[0] = exposure; return 1; }"
    renamed = good
    gsub(/rounded/, "stored", renamed)
    commented = good
    sub(/volatile double/, "double /* rounding boundary */ volatile", commented)
    direct = good
    sub(/exposure = rounded;/, "", direct)
    sub(/isfinite\(exposure\)/, "isfinite(rounded)", direct)
    if (!valid(good) || !valid(renamed) || !valid(commented) || !valid(direct)) {
        print "HDR rounding controls rejected a valid rounding boundary"
        failed = 1
    }
    bad = good; sub(/volatile /, "", bad); rejected(bad, "missing volatile")
    bad = good; sub(/volatile double/, "volatile long double", bad)
    rejected(bad, "extended precision storage")
    bad = good; sub(/volatile double/, "volatile float", bad)
    rejected(bad, "float storage")
    bad = good; sub(/exposure = rounded;/, "exposure = pow(2.0, exposure);", bad)
    rejected(bad, "bypassed store")
    bad = good; sub(/isfinite\(exposure\)/, "isfinite(count)", bad)
    rejected(bad, "unchecked result")
    p = index(good, "||")
    bad = substr(good, 1, p - 1) "&&" substr(good, p + 2)
    rejected(bad, "conditional rejection")
    bad = good; sub(/return 0;/, "return 1;", bad); rejected(bad, "accepted overflow")
    bad = good; sub(/rounded = pow/, "pixels[0] = 0; rounded = pow", bad)
    rejected(bad, "premature pixel write")
    bad = good; sub(/rounded = pow/, "if (0) rounded = pow", bad)
    rejected(bad, "conditional store")
    bad = good; sub(/rounded = pow/, "return 1; rounded = pow", bad)
    rejected(bad, "unreachable store")
    bad = good
    sub(/rounded = pow/, "if (!isfinite(exposure)) { return 0; } rounded = pow", bad)
    rejected(bad, "validation before rounding")
    bad = good; sub(/pixels\[0\]/, "exposure = 1.0; pixels[0]", bad)
    rejected(bad, "overwritten checked result")
    bad = good; sub(/volatile double rounded;/, "/* volatile double rounded; */", bad)
    rejected(bad, "commented declaration")
    rejected("/* " good " */", "commented function")
    rejected("\"" good "\"", "quoted function")
    rejected("", "missing function")
    if (failed) exit 1
}
{ source = source $0 "\n" }
END {
    if (failed) exit 1
    if (!valid(source)) {
        print FILENAME ": loader_hdr_finish must store pow in volatile double, read that value, and reject nonfinite exposure before touching pixels"
        exit 1
    }
}
