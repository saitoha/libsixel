# Extract TeX commands from GitHub-style Markdown math and reject commands
# outside the repository's GitHub.com-verified allowlist.

BEGIN {
    while ((getline allowlist_line < allowlist_path) > 0) {
        sub(/\r$/, "", allowlist_line)
        if (allowlist_line ~ /^[[:space:]]*#/ ||
            allowlist_line ~ /^[[:space:]]*$/) {
            continue
        }
        if (allowlist_line !~ /^[A-Za-z]+$/) {
            print allowlist_path ": invalid TeX command: " allowlist_line
            continue
        }
        allowed[allowlist_line] = 1
        allowed_count++
    }
    close(allowlist_path)
    if (allowed_count == 0) {
        print allowlist_path ": no allowed TeX commands"
    }
}

FNR == 1 {
    in_fence = 0
    fence_math = 0
    fence_char = ""
    fence_width = 0
    display_math = 0
}

{
    if (handle_fence($0)) {
        next
    }
    if (in_fence) {
        if (fence_math) {
            check_commands($0, FILENAME, FNR)
        }
        next
    }
    scan_prose($0, FILENAME, FNR)
}

function trim(value) {
    sub(/^[[:space:]]+/, "", value)
    sub(/[[:space:]]+$/, "", value)
    return value
}

function handle_fence(line, candidate, char, count, info) {
    candidate = line
    count = 0
    while (count < 3 && substr(candidate, 1, 1) == " ") {
        candidate = substr(candidate, 2)
        count++
    }

    char = substr(candidate, 1, 1)
    if (char != "`" && char != "~") {
        return 0
    }

    count = run_width(candidate, 1, char)
    if (count < 3) {
        return 0
    }
    info = substr(candidate, count + 1)

    if (in_fence) {
        if (char == fence_char && count >= fence_width &&
            info ~ /^[[:space:]]*$/) {
            in_fence = 0
            fence_math = 0
            fence_char = ""
            fence_width = 0
            return 1
        }
        return 0
    }

    if (char == "`" && index(info, "`") != 0) {
        return 0
    }

    in_fence = 1
    fence_char = char
    fence_width = count
    info = trim(info)
    fence_math = (info == "math")
    return 1
}

function run_width(line, start, char, count) {
    count = 0
    while (substr(line, start + count, 1) == char) {
        count++
    }
    return count
}

function is_escaped(line, position, count, index_) {
    count = 0
    index_ = position - 1
    while (index_ > 0 && substr(line, index_, 1) == "\\") {
        count++
        index_--
    }
    return count % 2
}

function find_dollar(line, start, index_) {
    for (index_ = start; index_ <= length(line); index_++) {
        if (substr(line, index_, 1) == "$" &&
            !is_escaped(line, index_)) {
            return index_
        }
    }
    return 0
}

function find_tick_close(line, start, width, index_, count) {
    for (index_ = start; index_ <= length(line); index_++) {
        if (substr(line, index_, 1) != "`") {
            continue
        }
        count = run_width(line, index_, "`")
        if (count == width) {
            return index_
        }
        index_ += count - 1
    }
    return 0
}

function check_commands(math, path, line_number, offset, remainder,
                        first, command, shown) {
    remainder = math
    while ((offset = index(remainder, "\\")) != 0) {
        remainder = substr(remainder, offset + 1)
        first = substr(remainder, 1, 1)
        if (first == "") {
            print path ":" line_number \
                ": trailing backslash in GitHub math"
            return
        }
        if (first ~ /[A-Za-z]/) {
            match(remainder, /^[A-Za-z]+/)
            command = substr(remainder, RSTART, RLENGTH)
        } else {
            command = first
        }
        if (!(command in allowed)) {
            shown = command
            if (shown == " ") {
                shown = "<space>"
            } else if (shown == "\t") {
                shown = "<tab>"
            }
            print path ":" line_number \
                ": GitHub math command \\" shown \
                " is not in the verified allowlist"
        }
        remainder = substr(remainder, length(command) + 1)
    }
}

function scan_prose(line, path, line_number, position, close_position,
                    count, math) {
    position = 1
    while (position <= length(line)) {
        if (display_math) {
            close_position = index(substr(line, position), "$$")
            if (close_position == 0) {
                check_commands(substr(line, position), path, line_number)
                return
            }
            close_position += position - 1
            math = substr(line, position, close_position - position)
            check_commands(math, path, line_number)
            display_math = 0
            position = close_position + 2
            continue
        }

        if (substr(line, position, 1) == "`") {
            count = run_width(line, position, "`")
            close_position = find_tick_close(line, position + count, count)
            if (close_position == 0) {
                position += count
                continue
            }
            position = close_position + count
            continue
        }

        if (substr(line, position, 1) != "$" ||
            is_escaped(line, position)) {
            position++
            continue
        }

        if (substr(line, position, 2) == "$`") {
            close_position = index(substr(line, position + 2), "`$")
            if (close_position == 0) {
                return
            }
            close_position += position + 1
            math = substr(line, position + 2,
                          close_position - position - 2)
            check_commands(math, path, line_number)
            position = close_position + 2
            continue
        }

        if (substr(line, position, 2) == "$$") {
            close_position = index(substr(line, position + 2), "$$")
            if (close_position == 0) {
                check_commands(substr(line, position + 2), path,
                               line_number)
                display_math = 1
                return
            }
            close_position += position + 1
            math = substr(line, position + 2,
                          close_position - position - 2)
            check_commands(math, path, line_number)
            position = close_position + 2
            continue
        }

        close_position = find_dollar(line, position + 1)
        if (close_position == 0) {
            position++
            continue
        }
        math = substr(line, position + 1,
                      close_position - position - 1)
        check_commands(math, path, line_number)
        position = close_position + 1
    }
}
