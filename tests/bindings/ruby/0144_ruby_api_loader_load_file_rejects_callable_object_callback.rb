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
    callback = Object.new
    def callback.call(_frame, _ctx)
      0
    end

    begin
      Libsixel::API.sixel_loader_load_file(loader, source, callback)
      puts 'not ok 1 - loader accepted callable object callback unexpectedly'
    rescue TypeError
      puts 'ok 1 - loader rejects callable object callback type'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader callable object callback rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
