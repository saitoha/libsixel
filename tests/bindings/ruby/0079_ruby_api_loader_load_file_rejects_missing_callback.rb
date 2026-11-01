#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  root = ENV.fetch('TOP_SRCDIR', Dir.pwd)
  source = File.join(root, 'tests/data/inputs/snake_64.png')

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    status = Libsixel::API.sixel_loader_load_file(loader, source, 0)
    if Libsixel::API.failed?(status)
      puts 'ok 1 - loader load_file rejects missing callback'
    else
      puts 'not ok 1 - loader load_file accepted missing callback'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader missing callback rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
