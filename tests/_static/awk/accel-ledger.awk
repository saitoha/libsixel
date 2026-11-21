# Compare acceleration units, their Accel tags, the acceleration ledger and
# the frozen list of untagged units.
#
# Usage: awk -v units=FILE -v legacy=FILE -f accel-ledger.awk LEDGER.md
# units is the output of accel-units.awk. Every problem is printed on one
# line; the exit status is 1 when any problem was found.

function fail(message) {
    print message
    failed = 1
}

function covered(family, backends) {
    if (index(" " backends " ", " " family " "))
        return 1
    return family == "sse2" && index(" " backends " ", " avx2 ")
}

BEGIN {
    FS = "\t"
    ncolumns = split("scalar neon sse2 avx2 wasm metal vulkan", column, " ")
    header = "| ID | Operation | Scalar | NEON | SSE2 | AVX2 | WASM | Metal | Vulkan | Notes |"
    in_ledger = 0
    seen_header = 0
}

/^<!-- accel-ledger -->$/ {
    in_ledger = 1
    next
}

/^<!-- accel-ledger-end -->$/ {
    in_ledger = 0
    closed = 1
    next
}

in_ledger {
    if ($0 == "")
        next
    if (!seen_header) {
        if ($0 != header)
            fail("ledger: unexpected table header: " $0)
        seen_header = 1
        next
    }
    if ($0 ~ /^\|( *-+ *\|)+$/)
        next
    count = split($0, cell, /[ \t]*\|[ \t]*/)
    # A row "| a | b |" splits into "", a, b, "".
    if (count != 12 || cell[1] != "" || cell[12] != "") {
        fail("ledger: malformed row: " $0)
        next
    }
    id = cell[2]
    if (id !~ /^[A-Z][A-Z0-9]*(-[A-Z0-9]+)*$/) {
        fail("ledger: malformed ID: " id)
        next
    }
    if (id in ledger_row)
        fail("ledger: duplicate ID " id)
    ledger_row[id] = FNR
    for (k = 1; k <= ncolumns; k++) {
        state = cell[k + 3]
        if (state !~ /^(done|planned|n\/a|rejected)$/)
            fail("ledger: " id " " column[k] " has unknown state \"" state "\"")
        ledger[id, column[k]] = state
    }
    next
}

END {
    if (!seen_header || !closed)
        fail("ledger: missing <!-- accel-ledger --> table")

    while ((getline line < legacy) > 0) {
        if (line ~ /^#/ || line == "")
            continue
        split(line, f, "\t")
        key = f[1] "\t" f[3]
        if (key in allowed)
            fail("legacy: duplicate row " f[1] " " f[3])
        allowed[key] = f[2] + 0
    }
    close(legacy)

    while ((getline line < units) > 0) {
        n = split(line, u, "\t")
        if (u[1] == "!malformed") {
            fail(u[2] ":" u[3] ": malformed Accel tag")
            continue
        }
        where = u[1] ":" u[2] " " u[3]
        if (u[5] == "") {
            untagged[u[1] "\t" u[3]]++
            untagged_where[u[1] "\t" u[3]] = where " (" u[4] ")"
            continue
        }
        delete backends_of
        ntags = split(u[5], tag, " ")
        backends = ""
        for (t = 1; t <= ntags; t++) {
            split(tag[t], pair, "/")
            backends = backends " " pair[2]
            if (!(pair[1] in ledger_row)) {
                fail(where ": Accel tag " pair[1] " is not in the ledger")
                continue
            }
            if (ledger[pair[1], pair[2]] != "done")
                fail(where ": Accel tag " pair[1] " " pair[2] \
                     " but the ledger says " ledger[pair[1], pair[2]])
            tagged[pair[1], pair[2]] = 1
        }
        nfam = split(u[4], fam, ",")
        for (k = 1; k <= nfam; k++)
            if (!covered(fam[k], backends))
                fail(where ": uses " fam[k] " but its Accel tag names" backends)
    }
    close(units)

    for (key in untagged) {
        if (!(key in allowed))
            fail(untagged_where[key] ": acceleration without an Accel tag")
        else if (untagged[key] > allowed[key])
            fail(untagged_where[key] ": more untagged units than the legacy list allows")
    }
    for (key in allowed) {
        if (!(key in untagged) || untagged[key] < allowed[key]) {
            split(key, f, "\t")
            fail("legacy: " f[1] " " f[2] " is no longer untagged; lower or remove its row")
        }
    }

    for (id in ledger_row)
        for (k = 2; k <= ncolumns; k++)
            if (ledger[id, column[k]] == "done" && !((id, column[k]) in tagged))
                fail("ledger: " id " " column[k] " is done but no Accel tag names it")

    exit failed
}
