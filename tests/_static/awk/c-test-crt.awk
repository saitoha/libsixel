# Shared C tests use portable adapters instead of deprecated MSVC CRT APIs.
# Catalogue scope: security-deprecated narrow/wide C CRT operations, plus
# legacy underscore spellings. POSIX aliases have a separate platform contract.
# Reference: https://learn.microsoft.com/en-us/cpp/c-runtime-library/security-features-in-the-crt
function strip(line,    result, token) {
    while (line != "") {
        if (comment) {
            if (!match(line, /\*\//)) break
            line = substr(line, RSTART + RLENGTH)
            comment = 0
        } else if (match(line, /"([^"\\]|\\.)*"|'([^'\\]|\\.)*'|\/\*|\/\//)) {
            result = result substr(line, 1, RSTART - 1)
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (token == "//") break
            if (token == "/*") comment = 1
            result = result " "
        } else {
            result = result line
            break
        }
    }
    return result
}
function inspect(text,    n, input, i, code) {
    comment = 0
    gsub(/\\\n/, "", text)
    n = split(text, input, "\n")
    for (i = 1; i <= n; ++i) code = code "\n" strip(input[i])
    # Match references as well as calls: aliases, addresses and parenthesized
    # callees must not evade the same source-level portability contract.
    return code ~ forbidden
}
BEGIN {
    names = "strcpy strcat strncpy strncat strtok strerror " \
        "wcscpy wcscat wcsncpy wcsncat wcstok " \
        "sprintf vsprintf _swprintf _vswprintf __swprintf_l __vswprintf_l " \
        "scanf fscanf sscanf vscanf vfscanf vsscanf " \
        "wscanf fwscanf swscanf vwscanf vfwscanf vswscanf " \
        "fopen freopen tmpfile tmpnam gets _getws " \
        "getenv _wgetenv localtime gmtime asctime ctime " \
        "_wasctime _wctime _ctime32 _ctime64 _wctime32 _wctime64 " \
        "_localtime32 _localtime64 _gmtime32 _gmtime64 " \
        "mbstowcs wcstombs wctomb mbsrtowcs wcsrtombs wcrtomb " \
        "_strerror _wcserror __wcserror _wfopen _wfreopen _wtmpnam " \
        "_snprintf _snwprintf _vsnprintf _vsnwprintf " \
        "_mbscpy _mbscat _mbsncpy _mbsncat _mbstok " \
        "_strset _strnset _strlwr _strupr _wcsset _wcsnset _wcslwr _wcsupr " \
        "_itoa _ltoa _ultoa _i64toa _ui64toa " \
        "_itow _ltow _ultow _i64tow _ui64tow"
    # List locale variants explicitly: conforming swprintf/vswprintf and
    # their count-taking locale forms are not legacy unbounded formatters.
    names = names " _sprintf_l _vsprintf_l _scanf_l _fscanf_l _sscanf_l " \
        "_vscanf_l _vfscanf_l _vsscanf_l _wscanf_l _fwscanf_l _swscanf_l " \
        "_vwscanf_l _vfwscanf_l _vswscanf_l _mbscpy_l _mbscat_l " \
        "_mbsncpy_l _mbsncat_l _mbstok_l _strlwr_l _strupr_l " \
        "_wcslwr_l _wcsupr_l _snprintf_l _vsnprintf_l " \
        "_snwprintf_l _vsnwprintf_l _mbstowcs_l _wcstombs_l _wctomb_l"
    pattern = names
    gsub(/ /, "|", pattern)
    forbidden = "(^|[^[:alnum:]_])(" pattern ")([^[:alnum:]_]|$)"
    count = split(names, functions, " ")
    for (i = 1; i <= count; ++i) {
        name = functions[i]
        if (!inspect(name "(arg);") ||
            !inspect(name "\n /* continued call */ (arg);") ||
            !inspect("#define alias " name) ||
            !inspect("callback = &" name ";") ||
            !inspect("(" name ")(arg);") ||
            inspect("sixel_compat_" name "(arg);") ||
            inspect(name "_s(arg);") ||
            inspect("/* " name "( */ puts(\"" name "(\"); // " name)) {
            print "CRT API controls failed for " name
            failed = 1
            exit 1
        }
    }
    if (!inspect("str\\\ncpy(arg);")) {
        print "CRT split identifier control failed"
        failed = 1
        exit 1
    }
    if (inspect("memcpy(dst, src, size); snprintf(dst, size, fmt);") ||
        inspect("swprintf(dst, size, fmt); vswprintf(dst, size, fmt, args);") ||
        inspect("_swprintf_l(dst, size, fmt, locale);")) {
        print "CRT allowed operation control failed"
        failed = 1
        exit 1
    }
}
# Scan each file once cheaply; only candidates need lexical stripping.
FNR == 1 { continued = "" }
{
    logical = continued $0
    if (sub(/\\$/, "", logical)) {
        continued = logical
        next
    }
    continued = ""
    if (logical ~ forbidden) candidates[FILENAME] = 1
}
END {
    for (file in candidates) {
        text = ""
        while ((status = getline line < file) > 0) text = text line "\n"
        close(file)
        if (status < 0) {
            print file ": cannot read source"
            failed = 1
        } else if (inspect(text)) {
            print file ": use a portable CRT adapter or bounded byte operation"
            failed = 1
        }
    }
    exit failed
}
