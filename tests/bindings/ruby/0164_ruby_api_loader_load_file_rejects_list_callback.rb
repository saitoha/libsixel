#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  source = File.expand_path('tests/data/inputs/snake_64.png', ENV.fetch('TOP_SRCDIR', Dir.pwd))
  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    rejected = false
    begin
      status = Libsixel::API.sixel_loader_load_file(loader, source, [])
      rejected = Libsixel::API.failed?(status)
    rescue TypeError, ArgumentError
      rejected = true
    end

    if rejected
      puts 'ok 1 - loader list callback rejection path verified'
    else
      puts 'not ok 1 - loader accepted list callback unexpectedly'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader list callback rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
