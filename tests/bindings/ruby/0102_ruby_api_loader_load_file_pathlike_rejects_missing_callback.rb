#!/usr/bin/env ruby
# frozen_string_literal: true

require 'pathname'

puts '1..1'

begin
  require 'libsixel'

  source = Pathname.new(File.expand_path('tests/data/inputs/snake_64.png', ENV.fetch('TOP_SRCDIR', Dir.pwd)))

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    status = Libsixel::API.sixel_loader_load_file(loader, source, 0)
    if Libsixel::API.failed?(status)
      puts 'ok 1 - loader load_file path-like input still rejects missing callback'
    else
      puts 'not ok 1 - loader load_file path-like input accepted missing callback'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader path-like missing-callback rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
