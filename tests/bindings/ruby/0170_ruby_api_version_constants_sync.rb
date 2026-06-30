#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel/constants'

  source_root = ENV.fetch('TOP_SRCDIR', Dir.pwd)
  build_root = ENV.fetch('TOP_BUILDDIR', source_root)
  header_path = [
    File.join(build_root, 'include', 'sixel.h'),
    File.join(source_root, 'include', 'sixel.h'),
    File.join(source_root, 'include', 'sixel.h.in')
  ].find { |path| File.file?(path) }
  expected = {}

  raise RuntimeError, 'public header was not found' unless header_path

  File.foreach(header_path) do |line|
    match = /^#define\s+(LIBSIXEL_(?:VERSION|ABI_VERSION))\s+"([^"]+)"/.match(line)
    next unless match

    expected[match[1]] = match[2]
  end

  raise RuntimeError, 'version constants were not found in header' unless expected.length == 2

  actual = {
    'LIBSIXEL_VERSION' => Libsixel::API::LIBSIXEL_VERSION,
    'LIBSIXEL_ABI_VERSION' => Libsixel::API::LIBSIXEL_ABI_VERSION
  }
  mismatched = expected.keys.reject { |name| actual[name] == expected[name] }

  unless mismatched.empty?
    detail = mismatched.map { |name| "#{name}=#{actual[name].inspect}, expected #{expected[name].inspect}" }
    raise RuntimeError, detail.join('; ')
  end

  puts 'ok 1 - version constants are synchronized with configured header'
rescue LoadError, StandardError => e
  puts 'not ok 1 - version constant synchronization check failed'
  puts "# #{e.class}: #{e.message}"
end
