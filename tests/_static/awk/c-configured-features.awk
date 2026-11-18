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
function decision(line,    code, words, count, i) {
    code = strip(line)
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
BEGIN {
    if (!decision("#ifdef _WIN32") ||
        !decision("#if defined(__GNUC__) && __GNUC__ >= 10") ||
        !decision("#if __has_builtin(__builtin_fma)") ||
        !decision("#define LOCAL_FMA FP_FAST_FMA") ||
        !decision("#if FP_FAST_FMA") ||
        !decision("#if(FP_FAST_FMA)") ||
        !decision("#if!defined(__CYGWIN__)") ||
        !decision("#if FLT_EVAL_METHOD == 0") ||
        !decision("#if __STDC_IEC_559__") ||
        !decision("#if __STDC_WANT_LIB_EXT1__") ||
        decision("#if HAVE_WORKING_FMA") ||
        decision("#if __STDC_VERSION__ >= 199901L") ||
        decision("#if !defined(__STDC_NO_THREADS__)") ||
        decision("#if __STDC_LIB_EXT1__ >= 201112L") ||
        decision("#if __STDC_VERSION_MATH_H__ >= 202311L") ||
        decision("#if __has_include(<threads.h>)") ||
        decision("#ifdef __cplusplus") ||
        decision("/* #if FP_FAST_FMA */") ||
        decision("char *s = \"#if __GNUC__\";")) {
        print "configured-feature scanner controls failed" > "/dev/stderr"
        failed = 1; exit 1
    }
    block_comment = 0
}
FNR == 1 {
    block_comment = 0; pending = ""
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
    expression = decision(pending)
    if (expression != "") print path "\t" first_line "\t" expression
    pending = ""
}
END { exit failed }
