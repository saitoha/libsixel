# Check literal raster fixture references without decoding image pixels.
# Historical oversized uses are pinned to the exact source line and dimensions;
# a new caller, duplicate use, changed operand or enlarged file is not inherited.
function quote(text) {
    gsub(/\047/, "\047\\\047\047", text)
    return "\047" text "\047"
}
function number(offset, count, little,    i, value) {
    value = 0
    for (i = 0; i < count; i++)
        value = value * 256 + bytes[offset + (little ? count - i - 1 : i)]
    return value
}
function dimensions(path,    command, line, i, n, words, offset, marker,
                    header_length, little, entries, tag, type, value, text) {
    width = height = 0
    if (path in widths) {
        width = widths[path]; height = heights[path]; return
    }
    for (i in bytes) delete bytes[i]
    # PNG dimensions occupy the fixed IHDR prefix. Other supported formats
    # need bounded header traversal; no image decoder or optional Python runs.
    header_length = path ~ /\.[pP][nN][gG]$/ ? 32 : 65536
    command = "od -An -v -tu1 -N " header_length " " quote(root "/" path) " 2>/dev/null"
    offset = 0
    while ((command | getline line) > 0) {
        n = split(line, words)
        for (i = 1; i <= n; i++)
            if (words[i] != "") bytes[offset++] = words[i] + 0
    }
    close(command)
    if (bytes[0] == 137 && bytes[1] == 80 && bytes[2] == 78 &&
            bytes[3] == 71 && offset >= 24) {
        width = number(16, 4, 0); height = number(20, 4, 0)
    } else if (bytes[0] == 71 && bytes[1] == 73 && bytes[2] == 70) {
        width = number(6, 2, 1); height = number(8, 2, 1)
    } else if (bytes[0] == 66 && bytes[1] == 77) {
        if (number(14, 4, 1) == 12) {
            width = number(18, 2, 1); height = number(20, 2, 1)
        } else {
            width = number(18, 4, 1); height = number(22, 4, 1)
            if (height >= 2147483648) height = 4294967296 - height
        }
    } else if (bytes[0] == 255 && bytes[1] == 216) {
        for (i = 2; i + 8 < offset;) {
            if (bytes[i++] != 255) break
            while (bytes[i] == 255) i++
            marker = bytes[i++]
            if (marker == 217 || marker == 218) break
            if (marker == 1 || (marker >= 208 && marker <= 215)) continue
            header_length = number(i, 2, 0)
            if (header_length < 2) break
            if (marker >= 192 && marker <= 207 && marker != 196 &&
                    marker != 200 && marker != 204) {
                height = number(i + 3, 2, 0)
                width = number(i + 5, 2, 0); break
            }
            i += header_length
        }
    } else if (bytes[0] == 82 && bytes[1] == 73 && bytes[8] == 87 &&
            bytes[9] == 69 && bytes[10] == 66 && bytes[11] == 80) {
        if (bytes[15] == 88) {
            width = number(24, 3, 1) + 1
            height = number(27, 3, 1) + 1
        } else if (bytes[15] == 76 && bytes[20] == 47) {
            value = number(21, 4, 1)
            width = value % 16384 + 1
            height = int(value / 16384) % 16384 + 1
        } else if (bytes[23] == 157 && bytes[24] == 1 && bytes[25] == 42) {
            width = number(26, 2, 1) % 16384
            height = number(28, 2, 1) % 16384
        }
    } else if ((bytes[0] == 73 && bytes[1] == 73) ||
            (bytes[0] == 77 && bytes[1] == 77)) {
        little = bytes[0] == 73
        i = number(4, 4, little); entries = number(i, 2, little); i += 2
        while (entries-- > 0 && i + 12 <= offset) {
            tag = number(i, 2, little); type = number(i + 2, 2, little)
            if ((tag == 256 || tag == 257) &&
                    number(i + 4, 4, little) == 1) {
                value = number(i + 8, type == 3 ? 2 : 4, little)
                if (tag == 256) width = value
                else height = value
            }
            i += 12
        }
    } else if (bytes[0] == 56 && bytes[1] == 66 &&
            bytes[2] == 80 && bytes[3] == 83) {
        height = number(14, 4, 0); width = number(18, 4, 0)
    } else if (path ~ /\.[tT][gG][aA]$/ && offset >= 18) {
        width = number(12, 2, 1); height = number(14, 2, 1)
    } else if (bytes[0] == 80 && bytes[1] >= 49 && bytes[1] <= 54) {
        text = ""
        for (i = 0; i < offset && i < 1024; i++) text = text sprintf("%c", bytes[i])
        gsub(/#[^\n]*\n/, " ", text)
        split(text, words, /[ \t\r\n]+/)
        width = words[2] + 0; height = words[3] + 0
    }
    widths[path] = width; heights[path] = height
}
function allowed(key, w, h, count) {
    return key in legacy && legacy_width[key] == w &&
        legacy_height[key] == h && count <= legacy[key]
}
function inspect(source, raw,    line, match_text, fixture, key, hash) {
    line = raw
    if (line ~ /^[ \t]*(#|\/\/|\/\*|\*)/) return
    while (match(tolower(line), /(images\/|tests\/)[A-Za-z0-9_.+\/-]+\.(png|jpg|jpeg|gif|bmp|webp|ppm|pgm|pbm|tga|tif|tiff|psd|psb)/)) {
        match_text = substr(line, RSTART, RLENGTH)
        line = substr(line, RSTART + RLENGTH)
        fixture = match_text
        dimensions(fixture)
        if (width <= 64 && height <= 64) continue
        hash = raw; gsub(/[ \t\r]+/, " ", hash)
        sub(/^ /, "", hash); sub(/ $/, "", hash)
        hash = sha256(hash)
        key = source "\t" fixture "\t" hash
        seen[key]++
        observed_width[key] = width; observed_height[key] = height
        if (inventory) continue
        if (!allowed(key, width, height, seen[key])) {
            print "# " source ":" source_line ": " fixture " is " width "x" height \
                "; use a fixture at most 64x64 or obtain explicit approval" > "/dev/stderr"
            failed = 1
        }
    }
}
BEGIN {
    if (control != "") {
        key = "caller\timage\tstatement"
        legacy[key] = 1; legacy_width[key] = 600; legacy_height[key] = 450
        if (control == "existing") exit !allowed(key, 600, 450, 1)
        if (control == "new-caller") exit allowed("other" key, 600, 450, 1)
        if (control == "larger") exit allowed(key, 640, 450, 1)
        if (control == "duplicate") exit allowed(key, 600, 450, 2)
        if (control == "changed-operand") exit allowed(key "changed", 600, 450, 1)
        exit 2
    }
    if (!inventory) {
        file = legacy_file != "" ? legacy_file : \
            root "/tests/_static/data/test-image-size-legacy.tsv"
        while ((getline row < file) > 0) {
            if (row ~ /^#/ || row == "") continue
            n = split(row, fields, "\t")
            key = fields[1] "\t" fields[2] "\t" fields[3]
            if (n != 6 || key in legacy || fields[4] !~ /^[0-9]+$/ ||
                    fields[5] !~ /^[0-9]+$/ || fields[6] !~ /^[1-9][0-9]*$/) {
                print "# Invalid image-size legacy record: " row > "/dev/stderr"
                failed = 1; continue
            }
            legacy[key] = fields[6] + 0
            legacy_width[key] = fields[4] + 0
            legacy_height[key] = fields[5] + 0
        }
        close(file)
    }
}
{
    source = substr($0, length(root) + 2)
    source_line = 0
    while ((getline raw < $0) > 0) {
        source_line++
        inspect(source, raw)
    }
    close($0)
}
END {
    if (control != "") exit
    if (inventory) {
        for (key in seen)
            print key "\t" observed_width[key] "\t" observed_height[key] "\t" seen[key]
    } else {
        for (key in legacy)
            if (!(key in seen) || seen[key] != legacy[key]) {
                print "# Stale image-size legacy record: " key > "/dev/stderr"
                failed = 1
            }
    }
    exit failed ? 1 : 0
}
