# Check tools/docs/doc-types.tsv against the Markdown documents listed in
# the file named by documents: one row per document, known values only.

function report(message) {
    print "line " NR ": " message
    failed = 1
}
BEGIN {
    while ((getline path < documents) > 0) {
        expected[path] = 1
    }
    close(documents)
    types["tutorial"] = types["how-to"] = 1
    types["reference"] = types["explanation"] = 1
    audiences["user"] = audiences["contributor"] = audiences["both"] = 1
}
NR == 1 {
    if ($0 != "path\ttype\taudience\tnote") {
        report("header must be path, type, audience, note")
    }
    next
}
{
    if (NF != 4) {
        report("expected 4 tab-separated fields, found " NF)
        next
    }
    if ($1 in seen) {
        report("duplicate row for " $1)
    }
    seen[$1] = 1
    if (!($1 in expected)) {
        report("no such document: " $1)
    }
    if (!($2 in types)) {
        report("unknown type for " $1 ": " $2)
    }
    if (!($3 in audiences)) {
        report("unknown audience for " $1 ": " $3)
    }
}
END {
    for (path in expected) {
        if (!(path in seen)) {
            print "missing row for " path
            failed = 1
        }
    }
    exit failed
}
