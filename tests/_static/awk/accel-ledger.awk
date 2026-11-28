# Compare acceleration units and their Accel tags with the acceleration ledger.
#
# Usage: awk -v units=FILE -f accel-ledger.awk LEDGER.md
# units is the output of accel-units.awk. Every problem is printed on one
# line; the exit status is 1 when any problem was found.

function fail(message) {
    print message
    failed = 1
}

function covered(family, backends, families) {
    if (family in backends)
        return 1
    return family == "sse2" && ("avx2" in backends) && ("avx2" in families)
}

BEGIN {
    FS = "\t"
    nbackends = split("scalar neon sse2 avx2 wasm metal vulkan webgpu",
                      backend, " ")
    columns["simd"] = "scalar neon sse2 avx2 wasm"
    columns["gpu"] = "scalar metal vulkan webgpu"
    gpu["metal"] = gpu["vulkan"] = gpu["webgpu"] = 1
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
    previous_id = ""
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
    else if (previous_id != "" && id < previous_id)
        fail("ledger: " id " is out of order in the " table " table")
    previous_id = id
    table_row[table, id] = FNR
    applies = 0
    for (k = 1; k <= ncolumns; k++) {
        state = cell[k + 3]
        if (state !~ /^(done|planned|n\/a|rejected)$/ &&
            !(state == "8bit" && (column[k] in gpu)))
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

    while ((read_status = (getline line < units)) > 0) {
        split(line, u, "\t")
        if (u[1] == "!malformed") {
            fail(u[2] ":" u[3] ": malformed Accel tag")
            continue
        }
        if (u[1] == "!misplaced") {
            fail(u[2] ":" u[3] ": place the Accel tag before the unit")
            continue
        }
        where = u[1] ":" u[2] " " u[3]
        delete families
        nfam = split(u[4], fam, ",")
        for (k = 1; k <= nfam; k++)
            families[fam[k]] = 1
        if ("unsupported" in families)
            fail(where ": SIMD expression has no recognized backend")
        if (u[5] == "" && nfam) {
            fail(where " (" u[4] "): acceleration without an Accel tag")
            continue
        }
        delete backends
        ntags = split(u[5], tag, " ")
        for (t = 1; t <= ntags; t++) {
            split(tag[t], pair, "/")
            family = pair[2]
            fp32 = sub(/-fp32$/, "", family)
            backends[family] = 1
            if (family != "scalar" && !(family in families)) {
                fail(where ": Accel tag names unused backend " pair[2])
                continue
            }
            if (!(pair[1] in ledger_row)) {
                fail(where ": Accel tag " pair[1] " is not in the ledger")
                continue
            }
            state = ledger[pair[1], family]
            if (state != "done" && !(state == "8bit" && !fp32))
                fail(where ": Accel tag " pair[1] " " pair[2] \
                     " but the ledger says " state)
            tagged[pair[1], pair[2]] = 1
        }
        for (k = 1; k <= nfam; k++)
            if (fam[k] != "unsupported" && !covered(fam[k], backends, families))
                fail(where ": uses " fam[k] " without a matching Accel tag")
    }
    if (read_status < 0)
        fail("cannot read acceleration units: " units)
    close(units)

    for (id in ledger_row) {
        for (k = 2; k <= nbackends; k++) {
            family = backend[k]
            state = ledger[id, family]
            if ((state == "done" || (state == "8bit" && (family in gpu))) &&
                !((id, family) in tagged))
                fail("ledger: " id " " family " is " state " but no Accel tag names it")
            if (state == "done" && (family in gpu) &&
                !((id, family "-fp32") in tagged))
                fail("ledger: " id " " family " is done but no FP32 Accel tag names it")
        }
    }

    exit failed
}
