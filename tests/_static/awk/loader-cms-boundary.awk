# Reject private evaluator references, including aliases and inactive branches.
# Ignore comments and literals, and join C backslash-newline continuations.
# This is an API ownership check, not a proof about arbitrary RGB arithmetic.

FNR == 1 {
    block_comment = 0
    quote = ""
    continued = ""
    first_line = 1
}
{
    if (continued == "") {
        first_line = FNR
    }
    line = continued $0
    sub(/\r$/, "", line)
    continued = ""
    if (line ~ /\\$/) {
        continued = substr(line, 1, length(line) - 1)
        next
    }
    while (length(line) != 0) {
        if (block_comment) {
            stop = index(line, "*/")
            if (!stop) {
                break
            }
            line = substr(line, stop + 2)
            block_comment = 0
            continue
        }
        if (quote != "") {
            if (!match(line, /\\.|["\047]/)) {
                break
            }
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (token == quote) {
                quote = ""
            }
            continue
        }
        if (!match(line, /\/\*|\/\/|["\047]|[A-Za-z_][A-Za-z0-9_]*/)) {
            break
        }
        token = substr(line, RSTART, RLENGTH)
        line = substr(line, RSTART + RLENGTH)
        if (token == "//") {
            break
        }
        if (token == "/*") {
            block_comment = 1
        } else if (token == "\"" || token == "\047") {
            quote = token
        } else if (token ~ /^sixel_icc_/ ||
                   token ~ /^cms[A-Za-z0-9_]*Transform[A-Za-z0-9_]*$/ ||
                   token ~ /^ColorSyncTransform[A-Za-z0-9_]*$/ ||
                   token ~ /^Create(MultiProfile)?ColorTransform[AW]?$/ ||
                   token == "TranslateColors" ||
                   token == "TranslateBitmapBits") {
            printf "# %s:%d: CMS bypass reference: %s\n", \
                FILENAME, first_line, token
            failed = 1
        }
    }
}
END {
    exit failed ? 1 : 0
}
