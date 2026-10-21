# Check Markdown documents under docs/ against tools/docs/length-limits.tsv.
#
# The limits file is the first operand; every later operand is a document.
# A document is measured in non-blank source lines, a section is the span of
# a level-2 heading including its subsections, and a paragraph is one prose
# source line (paragraph, list item or quote) measured in words.  Fenced
# code, display math, tables, headings, comments and image lines count
# toward document and section lines but are not paragraphs.
#
# A row whose path is "*" sets the default limit of its kind.  Any other row
# is an exception that raises the limit for one document, one section of a
# document, or every paragraph of a document.  An exception records the
# measured value exactly and may only exist while that value exceeds the
# default, so an oversized document can shrink but never grow back.

function report(message) {
    print message
    failed = 1
}
function finish_section() {
    if (section != "" && section_lines > section_max[FILE_KEY, section]) {
        section_max[FILE_KEY, section] = section_lines
        sections[FILE_KEY, section] = 1
    }
    section = ""
    section_lines = 0
}
function finish_document() {
    if (FILE_KEY == "") {
        return
    }
    finish_section()
    document_lines[FILE_KEY] = lines
    documents[FILE_KEY] = 1
}
function limit_for(path, kind, key) {
    if ((path SUBSEP kind SUBSEP key) in limit) {
        return limit[path, kind, key]
    }
    return limit["*", kind, "*"]
}
FNR == 1 && NR == 1 {
    limits_file = FILENAME
}
FILENAME == limits_file {
    if (FNR == 1) {
        if ($0 != "path\tkind\tkey\tlimit") {
            report(limits_file ": header must be path, kind, key, limit")
        }
        next
    }
    if (NF != 4 || $4 !~ /^[1-9][0-9]*$/) {
        report(limits_file ":" FNR ": expected path, kind, key and a positive limit")
        next
    }
    if ($2 != "document" && $2 != "section" && $2 != "paragraph") {
        report(limits_file ":" FNR ": unknown kind " $2)
        next
    }
    if ($2 != "section" && $3 != "*") {
        report(limits_file ":" FNR ": a " $2 " limit uses key *")
        next
    }
    if (($1 SUBSEP $2 SUBSEP $3) in limit) {
        report(limits_file ":" FNR ": duplicate limit")
    }
    limit[$1, $2, $3] = $4
    if ($1 != "*") {
        exception_line[$1, $2, $3] = FNR
    }
    next
}
FNR == 1 {
    finish_document()
    FILE_KEY = FILENAME
    lines = 0
    fence = 0
    math = 0
}
{
    text = $0
    if (text ~ /^[ \t]*(```|~~~)/) {
        fence = !fence
    } else if (!fence && text ~ /^[ \t]*\$\$[ \t]*$/) {
        math = !math
    } else if (text ~ /^[ \t]*$/) {
        next
    } else if (!fence && !math) {
        if (text ~ /^## /) {
            finish_section()
            section = substr(text, 4)
        }
        if (text !~ /^#/ && text !~ /^[ \t]*\|/ && \
            text !~ /^[ \t]*<!--/ && text !~ /^[ \t]*!\[/) {
            count = split(text, token, /[ \t]+/)
            words = 0
            for (i = 1; i <= count; i++) {
                if (token[i] != "") {
                    words++
                }
            }
            paragraph_limit = limit_for(FILENAME, "paragraph", "*")
            if (words > paragraph_words[FILENAME]) {
                paragraph_words[FILENAME] = words
            }
            if (words > paragraph_limit) {
                report(FILENAME ":" FNR ": paragraph has " words \
                    " words (limit " paragraph_limit ")")
            }
        }
    }
    lines++
    if (section != "") {
        section_lines++
    }
}
END {
    finish_document()
    for (path in documents) {
        value = document_lines[path]
        maximum = limit_for(path, "document", "*")
        if (value > maximum) {
            report(path ": document has " value " lines (limit " maximum ")")
        }
    }
    for (pair in sections) {
        split(pair, part, SUBSEP)
        value = section_max[part[1], part[2]]
        maximum = limit_for(part[1], "section", part[2])
        if (value > maximum) {
            report(part[1] ": section \"" part[2] "\" has " value \
                " lines (limit " maximum ")")
        }
    }
    for (triple in exception_line) {
        split(triple, part, SUBSEP)
        where = limits_file ":" exception_line[triple] ": "
        if (!(part[1] in documents)) {
            report(where "no such document " part[1])
            continue
        }
        if (part[2] == "document") {
            value = document_lines[part[1]]
        } else if (part[2] == "paragraph") {
            value = paragraph_words[part[1]]
        } else if ((part[1] SUBSEP part[3]) in sections) {
            value = section_max[part[1], part[3]]
        } else {
            report(where "no section \"" part[3] "\" in " part[1])
            continue
        }
        if (value <= limit["*", part[2], "*"]) {
            report(where "obsolete exception: " part[1] " " part[2] \
                " is within the default limit; remove the row")
        } else if (value < limit[triple]) {
            report(where "lower the exception for " part[1] " " part[2] \
                " to the measured " value)
        }
    }
    exit failed
}
