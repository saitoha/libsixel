#!/usr/bin/env ruby
# frozen_string_literal: true

require 'set'

if ARGV.length < 2 || ARGV.length > 3
  warn "usage: #{$PROGRAM_NAME} <coverage-dir> <output-lcov> [source-root]"
  exit 2
end

coverage_dir = ARGV[0]
output_lcov = ARGV[1]
source_root = ARGV[2] ? File.expand_path(ARGV[2]) : nil

lib_root = source_root ? File.expand_path('ruby/lib', source_root) : nil

records = Hash.new do |hash, key|
  hash[key] = { found: Set.new, hits: Hash.new(0) }
end

normalize_path = lambda do |path|
  normalized = path.to_s
  return normalized unless source_root

  # Map bundled gem paths back to source-tree ruby/lib for stable reports.
  match = normalized.match(%r{/libsixel-ruby-[^/]+/lib/(.+)\z})
  if match
    candidate = File.expand_path(File.join('ruby/lib', match[1]), source_root)
    normalized = candidate if File.file?(candidate)
  end
  normalized
end

Dir.glob(File.join(coverage_dir, '*.marshal')).sort.each do |marshal_path|
  data = Marshal.load(File.binread(marshal_path))
  next unless data.is_a?(Hash)

  data.each do |path, line_hits|
    next unless line_hits.is_a?(Array)

    normalized = normalize_path.call(path)
    if lib_root
      next unless normalized.start_with?("#{lib_root}/")
    end

    entry = records[normalized]
    line_hits.each_with_index do |hit, idx|
      next if hit.nil?
      line = idx + 1
      entry[:found].add(line)
      entry[:hits][line] += hit if hit.positive?
    end
  end
end

File.open(output_lcov, 'w') do |f|
  records.keys.sort.each do |file_path|
    entry = records[file_path]
    next if entry[:found].empty?

    lines_found = 0
    lines_hit = 0

    f.puts 'TN:'
    f.puts "SF:#{file_path}"
    entry[:found].to_a.sort.each do |line|
      hit = entry[:hits][line]
      f.puts "DA:#{line},#{hit}"
      lines_found += 1
      lines_hit += 1 if hit.positive?
    end
    f.puts "LF:#{lines_found}"
    f.puts "LH:#{lines_hit}"
    f.puts 'end_of_record'
  end
end
