# Source files consume configured capabilities instead of guessing from a
# compiler, CPU or OS name. Standard C availability macros remain permitted.
function forbidden(name) {
    if (name ~ /^__STDC_(VERSION|HOSTED|NO_ATOMICS|NO_COMPLEX|NO_THREADS|NO_VLA|LIB_EXT1)__$/ ||
        name ~ /^__STDC_VERSION_(ASSERT|COMPLEX|FENV|FLOAT|MATH|SETJMP|STDARG|STDATOMIC|STDBIT|STDCKDINT|STDDEF|STDINT|STDIO|STDLIB|STRING|TGMATH|TIME|UCHAR|WCHAR)_H__$/ ||
        name ~ /^(__STDC__|__cplusplus|__has_include|__has_embed|__has_c_attribute)$/)
        return 0
    # These are expansion operators, syntax or diagnostics, not probes.
    if (name ~ /^__(FILE|LINE|DATE|TIME|TIMESTAMP|COUNTER|FUNCTION|PRETTY_FUNCTION|VA_ARGS)__$/ ||
        name ~ /^__(VA_OPT__|func__|attribute__|declspec|cdecl|stdcall|fastcall|thiscall|vectorcall|inline|inline__|restrict|restrict__|extension__|thread)$/ ||
        name ~ /^__builtin_/) return 0
    return name ~ /^__/ ||
        name ~ /^(_WIN32|_WIN64|WIN32|WIN64|_AIX|_HPUX_SOURCE|VMS|MSDOS|unix|linux)$/ ||
        name ~ /^(_MSC_|_MSVC_|_M_|_POSIX_|TARGET_OS_|MAC_OS_X_VERSION_)/ ||
        name ~ /^FP_FAST_/ ||
        name ~ /^(FLT_EVAL_METHOD|FLT_ROUNDS|math_errhandling)$/
}
function strip(line,    code, pattern, token) {
    pattern = "\"([^\"\\\\]|\\\\.)*\"|\047([^\047\\\\]|\\\\.)*\047|/\\*|//"
    code = displayed = ""
    while (line != "") {
        if (block_comment) {
            if (!match(line, /\*\//)) break
            line = substr(line, RSTART + RLENGTH); block_comment = 0
        } else if (match(line, pattern)) {
            code = code substr(line, 1, RSTART - 1)
            displayed = displayed substr(line, 1, RSTART - 1)
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (token == "/*") {
                block_comment = 1; code = code " "; displayed = displayed " "
            } else if (token == "//") break
            else { code = code " "; displayed = displayed token }
        } else {
            code = code line; displayed = displayed line; break
        }
    }
    return code
}
function decision(code,    words, count, i) {
    if (code !~ /^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|elif|elifdef|elifndef|define)([^[:alnum:]_]|$)/)
        return ""
    sub(/^[[:space:]]*#[[:space:]]*/, "", code)
    # Do not classify the name being defined as its own capability probe.
    if (code ~ /^define[[:space:]]/)
        sub(/^define[[:space:]]+[[:alnum:]_]+/, "", code)
    count = split(code, words, /[^[:alnum:]_]+/)
    for (i = 1; i <= count; i++) {
        if (forbidden(words[i])) {
            gsub(/[[:space:]]/, "", displayed)
            return displayed
        }
    }
    return ""
}
# A guard must guarantee availability. An alternative arm of || is only
# sufficient when it also guarantees the same capability.
function requires_feature(expr, macro,    i, depth, first_close, and_at,
                           left, right) {
    gsub(/[[:space:]]/, "", expr)
    if (expr ~ /[?:]/) return 0
    if (expr == macro || expr == "defined(" macro ")" ||
        expr == "defined" macro) return 1
    for (i = 1; i <= length(expr); i++) {
        if (substr(expr, i, 1) == "(") depth++
        else if (substr(expr, i, 1) == ")") {
            depth--
            if (!depth && !first_close) first_close = i
        } else if (!depth && substr(expr, i, 2) == "||") {
            left = substr(expr, 1, i - 1)
            right = substr(expr, i + 2)
            return requires_feature(left, macro) &&
                requires_feature(right, macro)
        } else if (!depth && !and_at && substr(expr, i, 2) == "&&") {
            and_at = i
        }
    }
    if (and_at)
        return requires_feature(substr(expr, 1, and_at - 1), macro) ||
            requires_feature(substr(expr, and_at + 2), macro)
    if (substr(expr, 1, 1) == "(" && first_close == length(expr))
        return requires_feature(substr(expr, 2, length(expr) - 2), macro)
    return 0
}
function builtin_guarded(name,    expected, legacy, i) {
    expected = "HAVE_" toupper(substr(name, 3))
    legacy = "HAVE_" toupper(name)
    for (i = 1; i <= branch_depth; i++)
        if (requires_feature(branch[i], expected) ||
            requires_feature(branch[i], legacy)) return 1
    return 0
}
function builtin_violation(code,    directive, expr, words, count, i) {
    if (code ~ /^[[:space:]]*#/) {
        directive = code
        sub(/^[[:space:]]*#[[:space:]]*/, "", directive)
        expr = directive
        sub(/^[[:alpha:]]+[[:space:]]*/, "", expr)
        sub(/[^[:alpha:]].*$/, "", directive)
        if (directive == "if" || directive == "ifdef" ||
            directive == "ifndef") {
            branch_depth++
        } else if (directive == "endif") {
            branch_depth--
            return ""
        }
        if (directive ~ /^(if|elif|ifdef|elifdef)$/) {
            branch[branch_depth] = expr
        } else if (directive ~ /^(else|ifndef|elifndef)$/) {
            branch[branch_depth] = ""
        }
        if (directive ~ /^(if|elif|ifdef|elifdef|ifndef|elifndef|else)$/)
            return ""
    }
    if (code !~ /__builtin_/) return ""
    count = split(code, words, /[^[:alnum:]_]+/)
    for (i = 1; i <= count; i++)
        if (words[i] ~ /^__builtin_[[:alnum:]_]+$/ &&
            !builtin_guarded(words[i])) return words[i]
    return ""
}
function classify(line) {
    return decision(strip(line))
}
BEGIN {
    if (!classify("#ifdef _WIN32") ||
        !classify("#if defined(__GNUC__) && __GNUC__ >= 10") ||
        !classify("#if __has_builtin(__builtin_fma)") ||
        !classify("#define LOCAL_FMA FP_FAST_FMA") ||
        !classify("#if FP_FAST_FMA") ||
        !classify("#if(FP_FAST_FMA)") ||
        !classify("#if!defined(__CYGWIN__)") ||
        !classify("#if FLT_EVAL_METHOD == 0") ||
        !classify("#if __STDC_IEC_559__") ||
        !classify("#if __STDC_WANT_LIB_EXT1__") ||
        classify("#if HAVE_WORKING_FMA") ||
        classify("#if __STDC_VERSION__ >= 199901L") ||
        classify("#if !defined(__STDC_NO_THREADS__)") ||
        classify("#if __STDC_LIB_EXT1__ >= 201112L") ||
        classify("#if __STDC_VERSION_MATH_H__ >= 202311L") ||
        classify("#if __has_include(<threads.h>)") ||
        classify("#ifdef __cplusplus") ||
        classify("/* #if FP_FAST_FMA */") ||
        classify("char *s = \"#if __GNUC__\";")) {
        print "configured-feature scanner controls failed" > "/dev/stderr"
        failed = 1; exit 1
    }
    if (!builtin_violation("x = __builtin_ctzll(bits);") ||
        builtin_violation("#if (defined(HAVE_BUILTIN_CTZLL) && FOO)") ||
        builtin_violation("x = __builtin_ctzll(bits);") ||
        !builtin_violation("x = __builtin_clzll(bits);") ||
        builtin_violation("#if OTHER_FEATURE") ||
        builtin_violation("#else") ||
        builtin_violation("x = __builtin_ctzll(bits);") ||
        builtin_violation("#endif") ||
        builtin_violation("#elif HAVE_BUILTIN_CLZLL") ||
        builtin_violation("x = __builtin_clzll(bits);") ||
        builtin_violation("#else") ||
        !builtin_violation("x = __builtin_clzll(bits);") ||
        builtin_violation("#endif") ||
        builtin_violation("#if HAVE_BUILTIN_CTZLL || OTHER_FEATURE") ||
        !builtin_violation("x = __builtin_ctzll(bits);") ||
        builtin_violation("#endif") ||
        builtin_violation("#if !HAVE_BUILTIN_CTZLL") ||
        !builtin_violation("x = __builtin_ctzll(bits);") ||
        builtin_violation("#endif") ||
        builtin_violation(strip("/* __builtin_ctzll(bits) */")) ||
        builtin_violation(strip("char *s = \"__builtin_ctzll(bits)\";"))) {
        print "builtin capability guard controls failed" > "/dev/stderr"
        failed = 1; exit 1
    }
    block_comment = 0
}
FNR == 1 {
    block_comment = 0; pending = ""; branch_depth = 0
    path = FILENAME
    if (index(path, root "/") == 1) path = substr(path, length(root) + 2)
}
{
    if (pending == "") first_line = FNR
    line = $0
    sub(/\r$/, "", line)
    pending = pending line
    # C joins escaped physical lines before stripping comments.
    if (sub(/\\$/, "", pending)) next
    code = strip(pending)
    builtin = builtin_violation(code)
    if (builtin != "") {
        print path ":" first_line ": " builtin \
            " requires its configured HAVE_BUILTIN guard" > "/dev/stderr"
        failed = 1
    }
    expression = decision(code)
    if (expression != "") print path "\t" first_line "\t" expression
    pending = ""
}
END { exit failed }
