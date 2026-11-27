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
    nbackends = split("scalar neon sse2 avx2 wasm metal vulkan webgpu",
                      backend, " ")
    columns["simd"] = "scalar neon sse2 avx2 wasm"
    columns["gpu"] = "scalar metal vulkan webgpu"
    headers["simd"] = "| ID | Operation | Scalar | NEON | SSE2 | AVX2 | WASM | Notes |"
    headers["gpu"] = "| ID | Operation | Scalar | Metal | Vulkan | WebGPU | Notes |"
    table = ""
}

/^<!-- accel-ledger (simd|gpu) -->$/ {
    if (table != "")
        fail("ledger: " $0 " opens inside the " table " table")
    table = $0
    sub(/^<!-- accel-ledger /, "", table)
    sub(/ -->$/, "", table)
    if (table in opened)
        fail("ledger: duplicate " table " table")
    opened[table] = 1
    ncolumns = split(columns[table], column, " ")
    seen_header = 0
    next
}

/^<!-- accel-ledger-end -->$/ {
    if (table == "")
        fail("ledger: <!-- accel-ledger-end --> without an open table")
    else if (!seen_header)
        fail("ledger: " table " table has no header")
    else
        closed[table] = 1
    table = ""
    next
}

table != "" {
    if ($0 == "")
        next
    if (!seen_header) {
        if ($0 != headers[table])
            fail("ledger: unexpected " table " table header: " $0)
        seen_header = 1
        next
    }
    if ($0 ~ /^\|( *-+ *\|)+$/)
        next
    count = split($0, cell, /[ \t]*\|[ \t]*/)
    # A row "| a | b |" splits into "", a, b, "".
    if (count != ncolumns + 5 || cell[1] != "" || cell[count] != "") {
        fail("ledger: malformed row: " $0)
        next
    }
    id = cell[2]
    if (id !~ /^[A-Z][A-Z0-9]*(-[A-Z0-9]+)*$/) {
        fail("ledger: malformed ID: " id)
        next
    }
    if ((table, id) in table_row)
        fail("ledger: duplicate ID " id " in the " table " table")
    table_row[table, id] = FNR
    applies = 0
    for (k = 1; k <= ncolumns; k++) {
        state = cell[k + 3]
        if (state !~ /^(done|planned|n\/a|rejected)$/)
            fail("ledger: " id " " column[k] " has unknown state \"" state "\"")
        if (k > 1 && state != "n/a")
            applies = 1
        if (column[k] == "scalar" && (id, "scalar") in ledger &&
            ledger[id, "scalar"] != state)
            fail("ledger: " id " scalar differs between the tables")
        ledger[id, column[k]] = state
    }
    if (!applies)
        all_na[table, id] = 1
    ledger_row[id] = FNR
    next
}

END {
    if (table != "")
        fail("ledger: " table " table is not closed")
    if (!("simd" in closed))
        fail("ledger: missing <!-- accel-ledger simd --> table")
    if (!("gpu" in closed))
        fail("ledger: missing <!-- accel-ledger gpu --> table")
    # A table lists an operation only when one of its backends applies;
    # an operation with no backend at all stays in the SIMD table.
    for (id in ledger_row) {
        for (k = 2; k <= nbackends; k++)
            if (!((id, backend[k]) in ledger))
                ledger[id, backend[k]] = "n/a"
        if (("gpu", id) in all_na)
            fail("ledger: " id " has no GPU backend; remove its GPU row")
        else if (("simd", id) in all_na && ("gpu", id) in table_row)
            fail("ledger: " id " has no SIMD backend; remove its SIMD row")
    }

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
        for (k = 2; k <= nbackends; k++)
            if (ledger[id, backend[k]] == "done" && !((id, backend[k]) in tagged))
                fail("ledger: " id " " backend[k] " is done but no Accel tag names it")

    exit failed
}
