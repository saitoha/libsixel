#!/bin/sh
# Emit TAP for machine-readable 6cells IDL contract attributes.

set -eu

src_root=${1:-}

echo "1..8"

if test -z "$src_root"; then
    echo "not ok 1 - 6cells IDL documents contract attributes"
    echo "# src_root argument is required"
    echo "not ok 2 - component interfaces carry responsibility attributes"
    echo "# src_root argument is required"
    echo "not ok 3 - refcounted components mark ref and unref lifetime"
    echo "# src_root argument is required"
    echo "not ok 4 - borrowed views are paired with invalidation contracts"
    echo "# src_root argument is required"
    echo "not ok 5 - concrete providers use COM-style coclass ids"
    echo "# src_root argument is required"
    echo "not ok 6 - generated header expands forbidden state names"
    echo "# src_root argument is required"
    echo "not ok 7 - native allocator stays outside component generation"
    echo "# src_root argument is required"
    echo "not ok 8 - component allocator accessors stay removed"
    echo "# src_root argument is required"
    exit 1
fi

idl_file=$src_root/include/6cells.idl
header_file=$src_root/include/6cells.h
public_header_file=$src_root/include/sixel.h.in
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-6cells-contract-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

failed=0

if test ! -f "$idl_file" ||
    test ! -f "$header_file" ||
    test ! -f "$public_header_file"; then
    echo "not ok 1 - 6cells IDL documents contract attributes"
    echo "# missing IDL or header: $idl_file $header_file $public_header_file"
    echo "not ok 2 - component interfaces carry responsibility attributes"
    echo "# missing IDL or header: $idl_file $header_file $public_header_file"
    echo "not ok 3 - refcounted components mark ref and unref lifetime"
    echo "# missing IDL or header: $idl_file $header_file $public_header_file"
    echo "not ok 4 - borrowed views are paired with invalidation contracts"
    echo "# missing IDL or header: $idl_file $header_file $public_header_file"
    echo "not ok 5 - concrete providers use COM-style coclass ids"
    echo "# missing IDL or header: $idl_file $header_file $public_header_file"
    echo "not ok 6 - generated header expands forbidden state names"
    echo "# missing IDL or header: $idl_file $header_file $public_header_file"
    echo "not ok 7 - native allocator stays outside component generation"
    echo "# missing IDL or header: $idl_file $header_file $public_header_file"
    echo "not ok 8 - component allocator accessors stay removed"
    echo "# missing IDL or header: $idl_file $header_file $public_header_file"
    exit 1
fi

if awk '
BEGIN {
    required["[component]"] = 1
    required["[native]"] = 1
    required["[opaque]"] = 1
    required["[refcounted]"] = 1
    required["[responsibility(\"...\")]"] = 1
    required["[forbid_state(\"name\", ...)]"] = 1
    required["[classid(\"...\")]"] = 1
    required["[serviceid(\"...\")]"] = 1
    required["[lifetime(retained)] / [lifetime(release)]"] = 1
    required["[mutates]"] = 1
    required["[invalidates(name, ...)]"] = 1
    required["[borrows(name)]"] = 1
}
/^module[[:space:]]+sixel[[:space:]]*\{/ {
    in_intro = 0
}
NR == 1 {
    in_intro = 1
}
in_intro != 0 {
    if ($0 ~ /valid_until/) {
        forbidden = 1
    }
    for (name in required) {
        if (index($0, name) != 0) {
            seen[name] = 1
        }
    }
}
END {
    ok = forbidden == 0
    for (name in required) {
        if (seen[name] == 0) {
            ok = 0
            print "missing attribute documentation: " name
        }
    }
    if (forbidden != 0) {
        print "valid_until should not be documented"
    }
    exit ok ? 0 : 1
}
' "$idl_file" > "$tmpdir/doc.txt"; then
    echo "ok 1 - 6cells IDL documents contract attributes"
else
    echo "not ok 1 - 6cells IDL documents contract attributes"
    sed 's/^/# /' "$tmpdir/doc.txt"
    failed=1
fi

if awk '
BEGIN {
    required["factory"] = 1
    required["timeline_writer"] = 1
    required["timeline_logger"] = 1
    required["lookup_policy"] = 1
    required["dither_policy"] = 1
    required["chunk"] = 1
	    required["loader_component"] = 1
	    required["loader_manager"] = 1
	    required["frame"] = 1
	    required["palette"] = 1
	    forbid["chunk"] = "read_cursor decode_state"
	    forbid["frame"] = "lookup_policy dither_policy decode_state"
	    forbid["palette"] = "lookup_policy dither_policy input_pixels method_for_largest method_for_rep quality_mode force_palette use_reversible quantize_model final_merge_mode lut_policy"
	    forbid["timeline_writer"] = "frame_context job_context"
	    forbid["timeline_logger"] = "output_file global_writer"
}
function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}
{
    line = trim($0)
    if (line ~ /^\[[^]]+\]$/) {
        attrs = attrs line "\n"
        next
    }
    if (line == "" || line ~ /^\/\// || line ~ /^\/\*/) {
        next
    }
    if (line ~ /^interface[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        name = line
        sub(/^interface[ \t]+/, "", name)
        sub(/[ \t]*\{$/, "", name)
        if (name in required) {
            seen[name] = 1
            if (attrs !~ /\[component, refcounted\]/ ||
                attrs !~ /\[responsibility\(/) {
                print "missing component attrs: " name
            }
            if (name in forbid) {
                split(forbid[name], parts, / /)
                for (i in parts) {
                    if (attrs !~ parts[i]) {
                        print "missing forbid_state name: " name ":" parts[i]
                    }
                }
            }
        }
        attrs = ""
        next
    }
    attrs = ""
}
END {
    ok = 1
    for (name in required) {
        if (seen[name] == 0) {
            ok = 0
            print "missing component interface: " name
        }
    }
    exit ok ? 0 : 1
}
' "$idl_file" > "$tmpdir/components.txt"; then
    if test -s "$tmpdir/components.txt"; then
        echo "not ok 2 - component interfaces carry responsibility attributes"
        sed 's/^/# /' "$tmpdir/components.txt"
        failed=1
    else
        echo "ok 2 - component interfaces carry responsibility attributes"
    fi
else
    echo "not ok 2 - component interfaces carry responsibility attributes"
    sed 's/^/# /' "$tmpdir/components.txt"
    failed=1
fi

if awk '
BEGIN {
    required["factory"] = 1
    required["timeline_writer"] = 1
    required["timeline_logger"] = 1
    required["lookup_policy"] = 1
    required["dither_policy"] = 1
    required["chunk"] = 1
	    required["loader_component"] = 1
	    required["loader_manager"] = 1
	    required["frame"] = 1
	    required["palette"] = 1
}
function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}
{
    line = trim($0)
    if (line ~ /^interface[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        current = line
        sub(/^interface[ \t]+/, "", current)
        sub(/[ \t]*\{$/, "", current)
        in_interface = current in required
        attrs = ""
        next
    }
    if (in_interface == 0) {
        next
    }
    if (line == "};") {
        in_interface = 0
        current = ""
        attrs = ""
        next
    }
    if (line ~ /^\[[^]]+\]$/) {
        attrs = attrs line "\n"
        next
    }
    if (line ~ /^void[ \t]+ref[ \t]*\([ \t]*\)[ \t]*;$/) {
        if (attrs !~ /lifetime\(retained\)/) {
            print "missing retained lifetime: " current ".ref"
        }
        seen_ref[current] = 1
        attrs = ""
        next
    }
    if (line ~ /^void[ \t]+unref[ \t]*\([ \t]*\)[ \t]*;$/) {
        if (attrs !~ /lifetime\(release\)/) {
            print "missing release lifetime: " current ".unref"
        }
        seen_unref[current] = 1
        attrs = ""
        next
    }
    if (line != "" && line !~ /^\/\//) {
        attrs = ""
    }
}
END {
    for (name in required) {
        if (seen_ref[name] == 0) {
            print "missing ref method: " name
        }
        if (seen_unref[name] == 0) {
            print "missing unref method: " name
        }
    }
}
' "$idl_file" > "$tmpdir/lifetime.txt"; then
    if test -s "$tmpdir/lifetime.txt"; then
        echo "not ok 3 - refcounted components mark ref and unref lifetime"
        sed 's/^/# /' "$tmpdir/lifetime.txt"
        failed=1
    else
        echo "ok 3 - refcounted components mark ref and unref lifetime"
    fi
else
    echo "not ok 3 - refcounted components mark ref and unref lifetime"
    sed 's/^/# /' "$tmpdir/lifetime.txt"
    failed=1
fi

if awk '
function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}
{
    line = trim($0)
    if (line ~ /^interface[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        current = line
        sub(/^interface[ \t]+/, "", current)
        sub(/[ \t]*\{$/, "", current)
        attrs = ""
        next
    }
    if (line == "};") {
        current = ""
        attrs = ""
        next
    }
    if (line ~ /^\[[^]]+\]/) {
        attrs = attrs line "\n"
        next
    }
    if (current == "chunk" && line ~ /init_(source|memory)[ \t]*\(/ &&
        attrs ~ /mutates/ &&
        attrs ~ /invalidates\(bytes_view, source_path\)/) {
        seen[current "." line] = 1
    }
    if (current == "chunk" && line ~ /get_bytes[ \t]*\(/ &&
        attrs ~ /borrows\(bytes_view\)/) {
        seen["chunk.get_bytes"] = 1
    }
    if (current == "chunk" && line ~ /source_path[ \t]*\(/ &&
        attrs ~ /borrows\(source_path\)/) {
        seen["chunk.source_path"] = 1
    }
    if (current == "loader_manager" && line ~ /build_chain[ \t]*\(/ &&
        attrs ~ /invalidates\(loader_chain, selected_loader\)/) {
        seen["loader_manager.build_chain"] = 1
    }
    if (current == "loader_manager" && line ~ /load[ \t]*\(/ &&
        attrs ~ /borrows\(selected_loader\)/) {
        seen["loader_manager.load"] = 1
    }
    if (current == "frame" && line ~ /get_pixels[ \t]*\(/ &&
        attrs ~ /borrows\(pixels_view\)/) {
        seen["frame.get_pixels"] = 1
    }
	    if (current == "frame" &&
	        line ~ /(init_pixels|set_pixelformat|resize|resize_float32|clip)[ \t]*\(/ &&
	        attrs ~ /invalidates\(pixels_view\)/) {
	        seen["frame." line] = 1
	    }
	    if (current == "palette" &&
	        line ~ /(init_entries|generate)[ \t]*\(/ &&
	        attrs ~ /invalidates\(entries_view, float32_entries_view\)/) {
	        seen["palette." line] = 1
	    }
	    if (current == "palette" &&
	        line ~ /init_entries_float32[ \t]*\(/ &&
	        attrs ~ /invalidates\(float32_entries_view\)/) {
	        seen["palette." line] = 1
	    }
	    if (current == "palette" && line ~ /get_entries[ \t]*\(/ &&
	        attrs ~ /borrows\(entries_view\)/) {
	        seen["palette.get_entries"] = 1
	    }
	    if (current == "palette" &&
	        line ~ /get_entries_float32[ \t]*\(/ &&
	        attrs ~ /borrows\(float32_entries_view\)/) {
	        seen["palette.get_entries_float32"] = 1
	    }
    if (line != "" && line !~ /^\/\// && line !~ /^\[[^]]+\]$/) {
        attrs = ""
    }
}
END {
    required["chunk.SIXELSTATUS init_source(in chunk_source_request const *request);"] = 1
    required["chunk.SIXELSTATUS init_memory(in chunk_memory_request const *request);"] = 1
    required["chunk.get_bytes"] = 1
    required["chunk.source_path"] = 1
    required["loader_manager.build_chain"] = 1
    required["loader_manager.load"] = 1
    required["frame.get_pixels"] = 1
    required["frame.SIXELSTATUS init_pixels(in frame_pixels_request const *request);"] = 1
    required["frame.SIXELSTATUS set_pixelformat(in int pixelformat);"] = 1
	    required["frame.SIXELSTATUS resize("] = 1
	    required["frame.SIXELSTATUS resize_float32("] = 1
	    required["frame.SIXELSTATUS clip(in int x, in int y, in int width, in int height);"] = 1
	    required["palette.SIXELSTATUS init_entries(in palette_entries_request const *request);"] = 1
	    required["palette.SIXELSTATUS init_entries_float32("] = 1
	    required["palette.SIXELSTATUS generate(in palette_generate_request const *request);"] = 1
	    required["palette.get_entries"] = 1
	    required["palette.get_entries_float32"] = 1
    for (name in required) {
        if (seen[name] == 0) {
            print "missing borrow/invalidation contract: " name
        }
    }
}
' "$idl_file" > "$tmpdir/borrows.txt"; then
    if test -s "$tmpdir/borrows.txt"; then
        echo "not ok 4 - borrowed views are paired with invalidation contracts"
        sed 's/^/# /' "$tmpdir/borrows.txt"
        failed=1
    else
        echo "ok 4 - borrowed views are paired with invalidation contracts"
    fi
else
    echo "not ok 4 - borrowed views are paired with invalidation contracts"
    sed 's/^/# /' "$tmpdir/borrows.txt"
    failed=1
fi

if awk '
BEGIN {
    serviceid["factory_component"] = "services/factory"
    iface["factory_component"] = "factory"
    serviceid["timeline_writer_component"] = "services/timeline-writer"
    iface["timeline_writer_component"] = "timeline_writer"
    classid["chunk_component"] = "image/chunk"
    iface["chunk_component"] = "chunk"
    classid["timeline_logger_component"] = "diagnostics/timeline-logger"
    iface["timeline_logger_component"] = "timeline_logger"
    classid["frame_component"] = "image/frame"
    iface["frame_component"] = "frame"
	    classid["loader_manager_component"] = "loader/manager"
	    iface["loader_manager_component"] = "loader_manager"
	    classid["palette_component"] = "quant/palette"
	    iface["palette_component"] = "palette"
}
function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}
{
    line = trim($0)
    if (line ~ /^\[[^]]+\]$/) {
        attrs = attrs line "\n"
        next
    }
    if (line ~ /^coclass[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\{$/) {
        current = line
        sub(/^coclass[ \t]+/, "", current)
        sub(/[ \t]*\{$/, "", current)
        if (current in classid) {
            seen[current] = 1
            if (attrs !~ "classid\\(\"" classid[current] "\"\\)") {
                print "missing classid: " current
            }
        } else if (current in serviceid) {
            seen[current] = 1
            if (attrs !~ "serviceid\\(\"" serviceid[current] "\"\\)") {
                print "missing serviceid: " current
            }
        }
        attrs = ""
        next
    }
    if (line == "};") {
        current = ""
        attrs = ""
        next
    }
    if ((current in classid || current in serviceid) &&
        line ~ /^\[default\][ \t]+interface[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*;$/) {
        member = line
        sub(/^\[default\][ \t]+interface[ \t]+/, "", member)
        sub(/[ \t]*;$/, "", member)
        if (member == iface[current]) {
            default_seen[current] = 1
        } else {
            print "wrong default interface: " current ":" member
        }
    }
    if (line != "" && line !~ /^\/\// && line !~ /^\[[^]]+\]$/) {
        attrs = ""
    }
}
END {
    for (name in classid) {
        if (seen[name] == 0) {
            print "missing coclass: " name
        }
        if (default_seen[name] == 0) {
            print "missing default interface: " name
        }
    }
    for (name in serviceid) {
        if (seen[name] == 0) {
            print "missing service coclass: " name
        }
        if (default_seen[name] == 0) {
            print "missing default service interface: " name
        }
    }
}
' "$idl_file" > "$tmpdir/coclass.txt"; then
    if test -s "$tmpdir/coclass.txt"; then
        echo "not ok 5 - concrete providers use COM-style coclass ids"
        sed 's/^/# /' "$tmpdir/coclass.txt"
        failed=1
    else
        echo "ok 5 - concrete providers use COM-style coclass ids"
    fi
else
    echo "not ok 5 - concrete providers use COM-style coclass ids"
    sed 's/^/# /' "$tmpdir/coclass.txt"
    failed=1
fi

if awk '
BEGIN {
    required["read_cursor"] = 1
    required["decode_state"] = 1
    required["lookup_policy"] = 1
    required["dither_policy"] = 1
    required["frame_context"] = 1
    required["job_context"] = 1
    required["output_file"] = 1
	    required["global_writer"] = 1
	    required["input_pixels"] = 1
	    required["method_for_largest"] = 1
	    required["method_for_rep"] = 1
	    required["quality_mode"] = 1
	    required["force_palette"] = 1
	    required["use_reversible"] = 1
	    required["quantize_model"] = 1
	    required["final_merge_mode"] = 1
	    required["lut_policy"] = 1
}
/IDL forbidden state:/ {
    in_forbid = 1
    next
}
in_forbid != 0 && /\*\// {
    in_forbid = 0
    next
}
in_forbid != 0 {
    for (name in required) {
        if ($0 ~ "\\* - " name "$") {
            seen[name] = 1
        }
    }
}
END {
    for (name in required) {
        if (seen[name] == 0) {
            print "missing generated forbidden state bullet: " name
        }
    }
}
' "$header_file" > "$tmpdir/header-forbid.txt"; then
    if test -s "$tmpdir/header-forbid.txt"; then
        echo "not ok 6 - generated header expands forbidden state names"
        sed 's/^/# /' "$tmpdir/header-forbid.txt"
        failed=1
    else
        echo "ok 6 - generated header expands forbidden state names"
    fi
else
    echo "not ok 6 - generated header expands forbidden state names"
    sed 's/^/# /' "$tmpdir/header-forbid.txt"
    failed=1
fi

if awk -v idl_file="$idl_file" \
    -v header_file="$header_file" \
    -v public_header_file="$public_header_file" '
function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}
FILENAME == idl_file {
    line = trim($0)
    if (line ~ /^\[[^]]+\]$/) {
        attrs = attrs line "\n"
        next
    }
    if (line ~ /^typedef[ \t]+sixel_allocator_t[ \t]+allocator[ \t]*;$/) {
        seen_typedef = 1
        if (attrs !~ /\[native, opaque, refcounted\]/) {
            print "allocator typedef is not native opaque refcounted"
        }
        if (attrs !~ /responsibility\("provide allocation services required by factory creation"\)/) {
            print "allocator typedef is missing responsibility"
        }
        attrs = ""
        next
    }
    if (line ~ /^interface[ \t]+allocator([ \t]*\{|[ \t]*;)/) {
        print "allocator must not be generated as an interface"
    }
    if (line ~ /^coclass[ \t]+allocator/) {
        print "allocator must not be generated as a coclass"
    }
    if (line ~ /^interface[ \t]+factory[ \t]*\{$/) {
        in_factory = 1
        attrs = ""
        next
    }
    if (in_factory != 0 && line == "};") {
        in_factory = 0
        attrs = ""
        next
    }
    if (in_factory != 0 && line ~ /in allocator[ \t]+\*allocator/) {
        seen_factory_allocator = 1
    }
    if (line != "" && line !~ /^\/\// && line !~ /^\/\*/) {
        attrs = ""
    }
    next
}
FILENAME == header_file {
    if ($0 ~ /typedef[ \t]+sixel_allocator_t[ \t]+sixel_allocator_t;/) {
        print "generated header emitted duplicate allocator typedef"
    }
    if ($0 ~ /sixel_allocator_(interface|vtbl)/) {
        print "generated header emitted allocator interface artifacts"
    }
    if ($0 ~ /provide allocation services required by factory creation/) {
        seen_header_contract = 1
    }
    next
}
FILENAME == public_header_file {
    if ($0 ~ /sixel_allocator_ref[ \t]*\(/) {
        seen_allocator_ref = 1
    }
    if ($0 ~ /sixel_allocator_unref[ \t]*\(/) {
        seen_allocator_unref = 1
    }
}
END {
    if (seen_typedef == 0) {
        print "missing native allocator typedef"
    }
    if (seen_factory_allocator == 0) {
        print "factory create must use allocator alias"
    }
    if (seen_header_contract == 0) {
        print "generated header is missing allocator contract comment"
    }
    if (seen_allocator_ref == 0) {
        print "public allocator ref function is missing"
    }
    if (seen_allocator_unref == 0) {
        print "public allocator unref function is missing"
    }
}
' "$idl_file" "$header_file" "$public_header_file" \
    > "$tmpdir/native-allocator.txt"; then
    if test -s "$tmpdir/native-allocator.txt"; then
        echo "not ok 7 - native allocator stays outside component generation"
        sed 's/^/# /' "$tmpdir/native-allocator.txt"
        failed=1
    else
        echo "ok 7 - native allocator stays outside component generation"
    fi
else
    echo "not ok 7 - native allocator stays outside component generation"
    sed 's/^/# /' "$tmpdir/native-allocator.txt"
    failed=1
fi

if find "$src_root/include" "$src_root/src" "$src_root/tests" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.idl' \
        -o -name '*.t' \) \
    ! -path "$src_root/tests/_static/*" \
    -exec awk -v idl_file="$idl_file" '
function trim(text) {
    gsub(/^[ \t]+/, "", text)
    gsub(/[ \t]+$/, "", text)
    return text
}
{
    line = trim($0)
    if (FILENAME == idl_file) {
	        if (line ~ /^interface[ \t]+(output|frame|chunk|palette)[ \t]*\{$/) {
            current = line
            sub(/^interface[ \t]+/, "", current)
            sub(/[ \t]*\{$/, "", current)
            next
        }
        if (line == "};") {
            current = ""
            next
        }
        if (current != "" && line ~ /allocator[ \t]*\([ \t]*\)/) {
            print FILENAME ":" FNR ": allocator method on " current
        }
    }
	    if ($0 ~ /sixel_(output|frame|chunk|palette)_get_allocator[ \t]*\(/) {
        print FILENAME ":" FNR ": forbidden allocator getter export"
    }
	    if ($0 ~ /sixel_(output|frame|chunk|palette)_vtbl_allocator/) {
        print FILENAME ":" FNR ": forbidden allocator vtbl shim"
    }
    if ($0 ~ /->[ \t]*vtbl->[ \t]*allocator/) {
        print FILENAME ":" FNR ": forbidden allocator vtbl call"
    }
}
' {} + > "$tmpdir/allocator-accessors.txt"; then
    if test -s "$tmpdir/allocator-accessors.txt"; then
        echo "not ok 8 - component allocator accessors stay removed"
        sed 's/^/# /' "$tmpdir/allocator-accessors.txt"
        failed=1
    else
        echo "ok 8 - component allocator accessors stay removed"
    fi
else
    echo "not ok 8 - component allocator accessors stay removed"
    sed 's/^/# /' "$tmpdir/allocator-accessors.txt"
    failed=1
fi

exit "$failed"
