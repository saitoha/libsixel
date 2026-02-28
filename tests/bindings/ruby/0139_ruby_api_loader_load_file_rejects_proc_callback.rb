#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  source = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    begin
      Libsixel::API.sixel_loader_load_file(loader, source, proc { |_f, _f2| 0 })
      puts 'not ok 1 - loader load_file accepted Proc callback'
    rescue TypeError
      puts 'ok 1 - loader load_file rejects Proc callback type'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader Proc callback rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
