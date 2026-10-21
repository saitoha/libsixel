# Reject literal Unix stream aliases passed to native file APIs or logging.
# Policy: docs/misc/platforms/openvms.md

function uncomment(text, c_source,    out, pattern, token) {
    out = ""
    # Consume quoted strings before comment tokens so paths inside strings
    # survive. Token-sized steps avoid a costly character-by-character scan.
    pattern = "\"([^\"\\\\]|\\\\.)*\"|\047([^\047\\\\]|\\\\.)*\047|"
    pattern = pattern (c_source ? "/\\*([^*]|\\*+[^*/])*\\*+/|//[^\n]*" : "#[^\n]*")
    while (match(text, pattern)) {
        out = out substr(text, 1, RSTART - 1)
        token = substr(text, RSTART, RLENGTH)
        text = substr(text, RSTART + RLENGTH)
        out = out (token ~ /^[\"\047]/ ? token : " ")
    }
    return out text
}

function forbidden(text, c_source) {
    # Most files cannot contain a violation; avoid tokenizing those sources.
    if (text !~ /\/dev\/(std|fd\/)|\/proc\/self\/fd\//) return 0
    text = uncomment(text, c_source)
    # Shell and Make continuations must not hide a literal path.
    gsub(/\\\n/, "", text)
    return (c_source && text ~ native_call) || text ~ log_setting
}

function check_file(    source, line_number) {
    if (!candidate) return
    source = ""
    for (line_number = 1; line_number <= line_count; line_number++)
        source = source lines[line_number] "\n"
    if (file != "" && forbidden(source, file ~ /\.(c|h|cc|cpp|m)(\.in)?$/)) {
        print "# " file ": native stream alias; use a regular log file or an existing stream"
        failed = 1
    }
}

BEGIN {
    alias = "(/dev/(stdin|stdout|stderr|fd/[0-9]+)|/proc/self/fd/[0-9]+)"
    native_call = "(^|[^[:alnum:]_])(fopen|freopen|open|open64|sixel_compat_fopen)" \
        "[[:space:]]*\\([[:space:]]*\"" alias "\""
    log_setting = "(^|[^[:alnum:]_])SIXEL_LOG_PATH[\"\047]?[[:space:]]*([:+?]?=|[:,])" \
        "[[:space:]]*[\"\047]?" alias "([\"\047[:space:]);}]|$)"
    if (selftest) {
        # Exercise the matcher without creating fixture files or processes.
        bad[1] = "fopen(\n/* path */ \"/dev/stderr\", \"w\");"
        bad[2] = "freopen(\"/dev/stdout\", \"w\", stdout);"
        bad[3] = "sixel_compat_fopen(\"/dev/fd/2\", \"w\");"
        bad[4] = "open(\"/proc/self/fd/1\", 0);"
        bad[5] = "SIXEL_LOG_PATH=/dev/stderr img2sixel"
        bad[6] = "export SIXEL_LOG_PATH=\"/dev/stdout\""
        bad[7] = "env.set(\047SIXEL_LOG_PATH\047,\n\047/dev/fd/2\047)"
        bad[8] = "\047SIXEL_LOG_PATH\047: \047/proc/self/fd/2\047"
        bad[9] = "SIXEL_LOG_PATH=\\\n/dev/stderr"
        bad[10] = "SIXEL_LOG_PATH := /dev/stdin"
        bad[11] = "SIXEL_LOG_PATH ?= /dev/fd/0"
        good[1] = "fopen(\"timeline.jsonl\", \"w\");"
        good[2] = "fopen(\"/dev/null\", \"w\");"
        good[3] = "/* fopen(\"/dev/stderr\", \"w\"); */"
        good[4] = "// fopen(\"/dev/stderr\", \"w\");\n"
        good[5] = "SIXEL_LOG_PATH=\"${ARTIFACT_LOCAL_DIR}/trace.jsonl\""
        good[6] = "img2sixel 2>&1 >/dev/null"
        good[7] = "# SIXEL_LOG_PATH=/dev/stderr\n"
        good[8] = "SIXEL_LOG_PATH=/dev/stderr-file"
        good[9] = "UNRELATED_SIXEL_LOG_PATH=/dev/stderr"
        for (i in bad) if (!forbidden(bad[i], i <= 4)) exit 1
        for (i in good) if (forbidden(good[i], i <= 4)) exit 1
        exit 0
    }
}

FNR == 1 { check_file(); file = FILENAME; line_count = candidate = 0 }
{
    lines[++line_count] = $0
    if ($0 ~ /\/dev\/(std|fd\/)|\/proc\/self\/fd\//) candidate = 1
}
END { check_file(); if (failed) exit 1 }
