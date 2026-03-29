#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  source_root = ENV.fetch('TOP_SRCDIR', Dir.pwd)
  header_path = File.join(source_root, 'include', 'sixel.h.in')
  expected = []

  File.foreach(header_path) do |line|
    match = /^#define\s+(SIXEL_[A-Z0-9_]+)\s+/.match(line)
    next unless match

    name = match[1]
    next unless name.start_with?('SIXEL_LOADER_OPTION_',
                                 'SIXEL_LUT_POLICY_',
                                 'SIXEL_COLORSPACE_')

    expected << name
  end
  expected.uniq!

  missing = expected.reject { |name| Libsixel::API.const_defined?(name, false) }
  raise RuntimeError, "missing constants: #{missing.join(', ')}" unless missing.empty?

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    status = Libsixel::API.sixel_loader_setopt(
      loader,
      Libsixel::API::SIXEL_LOADER_OPTION_START_FRAME_NO,
      '0'
    )
    raise RuntimeError, "sixel_loader_setopt failed: #{status}" if Libsixel::API.failed?(status)
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end

  puts 'ok 1 - setopt constants are synchronized with header'
rescue StandardError => e
  puts 'not ok 1 - setopt constant synchronization check failed'
  puts "# #{e.class}: #{e.message}"
end
