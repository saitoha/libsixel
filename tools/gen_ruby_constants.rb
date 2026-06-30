#!/usr/bin/env ruby
# Generate ruby constants from include/sixel.h

require 'set'

abort("usage: #{$0} <input sixel.h> <output constants.rb>") unless ARGV.size == 2
in_h, out_rb = ARGV

text = File.read(in_h)

logical_lines = []
pending = +''
text.each_line do |raw_line|
  line = raw_line.chomp
  if pending.empty?
    pending = line
  else
    pending << ' ' << line.lstrip
  end

  # Public header constants sometimes use C preprocessor continuations to
  # alias one macro to another.  Parse those as a single logical line so the
  # resolver below can follow the alias instead of seeing a bare backslash.
  if pending.end_with?('\\')
    pending = pending[0...-1].rstrip
    next
  end

  logical_lines << pending
  pending = +''
end
logical_lines << pending unless pending.empty?

defs = {}
order = []

logical_lines.each do |line|
  line = line.strip
  next unless line.start_with?('#define ')
  line = line.sub(/^#define\s+/, '')
  # skip function-like macros only (e.g. NAME(arg)), not parenthesized values
  next if line =~ /^\w+\(/
  name, val = line.split(/\s+/, 2)
  next if name.nil? || val.nil?
  name = name.strip
  val = val.strip
  next unless name =~ /^(SIXEL_|LIBSIXEL_)/
  # strip trailing comments (single-line and multi-line starts)
  val = val.sub(%r{/\*.*}, '').strip
  order << name unless defs.key?(name)
  defs[name] = val
end

resolved = {}
max_pass = 10
numeric = /^([0-9]+|0x[0-9a-fA-F]+)$/

def safe_eval(expr)
  # allow digits, hex, ops, spaces, parens
  raise 'unsafe expr' unless expr =~ /\A[0-9xXa-fA-F\s\(\)\|\&\^~<>\?\+\-\*\/]+\z/
  Integer(eval(expr))
end

max_pass.times do
  progress = false
  order.each do |k|
    next if resolved.key?(k)
    v = defs[k]
    if v.start_with?('"') || v.start_with?("'")
      resolved[k] = v
      progress = true
      next
    end

    # Convert parenthesized C character literals such as ('d') into
    # Ruby string literals so option flag constants keep their semantics.
    char_match = /\A\(?\s*'((?:\\.|[^']))'\s*\)?\z/.match(v)
    if char_match
      c = char_match[1]
      c = c.gsub('\\', '\\\\').gsub('"', '\\"')
      resolved[k] = '"' + c + '"'
      progress = true
      next
    end

    expr = v.dup
    # normalize integer suffixes (UL, U, L)
    expr.gsub!(/([0-9])([uUlL]+)/, '\\1')
    # replace known symbols
    defs.keys.each do |sym|
      next unless expr.include?(sym)
      if resolved.key?(sym) && resolved[sym].to_s =~ numeric
        expr = expr.gsub(/\b#{Regexp.escape(sym)}\b/, resolved[sym].to_s)
      end
    end
    begin
      if expr =~ numeric
        resolved[k] = Integer(expr)
        progress = true
      else
        val = safe_eval(expr)
        resolved[k] = val
        progress = true
      end
    rescue
      # keep for next pass
    end
  end
  break unless progress
end

# Anything remaining: keep as raw string
order.each { |k| resolved[k] ||= defs[k] }

File.open(out_rb, 'w') do |f|
  f.puts "# Auto-generated from #{File.basename(in_h)}. Do not edit."
  f.puts "module Libsixel\n  module API"
  order.each do |k|
    v = resolved[k]
    if v.is_a?(String) && v.start_with?('"')
      f.puts "    #{k} = #{v}"
    elsif v.is_a?(String) && v =~ /^0x[0-9a-fA-F]+$/
      f.puts "    #{k} = #{v}"
    else
      f.puts "    #{k} = #{v.to_i}"
    end
  end
  f.puts "  end\nend\n"
end
