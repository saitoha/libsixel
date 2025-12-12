#!/usr/bin/env awk -f
# Generate completion_embed.h from bash/zsh completion sources.
#
# ASCII pipeline summary:
#
#   bash/img2sixel ----\
#                        >--[gen_completion_embed.awk]--> completion_embed.h
#   zsh/_img2sixel  ----/
#
# The script accepts two arguments:
#   1. Path to the bash completion file
#   2. Path to the zsh completion file
# It writes the resulting header to STDOUT so that callers can redirect it.
#
# Long literals are broken into 256-byte slices to avoid triggering
# -Woverlength-strings on strict ISO C99 compilers.

function file_size(path,    cmd, size_str, status)
{
        cmd = "wc -c < '" path "'"
        size_str = ""
        status = (cmd | getline size_str)
        close(cmd)
        if (status <= 0) {
                print "gen_completion_embed.awk: failed to stat " path > \
                    "/dev/stderr"
                exit 1
        }
        sub(/^ +/, "", size_str)
        sub(/ +$/, "", size_str)
        return size_str + 0
}

function escape_text(text)
{
        gsub(/\\/, "\\\\", text)
        gsub(/"/, "\\\"", text)
        return text
}

function emit_literal_chunks(text,
    chunk_limit, start, chunk)
{
        chunk_limit = 256;
        start = 1;

        while (start <= length(text)) {
                chunk = substr(text, start, chunk_limit);
                printf "\"%s\"\n", chunk;
                start += chunk_limit;
        }
}

function emit_lines(path, symbol,
    total_bytes, line_count, text_bytes, idx, has_trailing_newline)
{
        total_bytes = file_size(path)
        line_count = 0
        text_bytes = 0
        while ((getline line < path) > 0) {
                line_count++
                text_bytes += length(line)
                lines[line_count] = line
        }
        close(path)

        has_trailing_newline = 0
        if (line_count == 0) {
                has_trailing_newline = 1
        } else if (total_bytes == text_bytes + line_count) {
                has_trailing_newline = 1
        }

        print "static const char " symbol "[] ="
        if (line_count == 0) {
                print "\"\""
        }

        for (idx = 1; idx <= line_count; idx++) {
                text = escape_text(lines[idx])
                if (idx < line_count) {
                        text = text "\\n"
                } else if (has_trailing_newline) {
                        text = text "\\n"
                }
                emit_literal_chunks(text)
                delete lines[idx]
        }

        print "\"\";"
        print ""
}

BEGIN {
        if (ARGC != 3) {
                print "Usage: gen_completion_embed.awk BASH ZSH" > \
                    "/dev/stderr"
                exit 1
        }

        bash_path = ARGV[1]
        zsh_path = ARGV[2]
        ARGV[1] = ""
        ARGV[2] = ""

        print "/* auto-generated; do not edit */"
        print "#pragma once"
        print ""

        emit_lines(bash_path, "img2sixel_bash_completion")
        emit_lines(zsh_path, "img2sixel_zsh_completion")

        exit 0
}
