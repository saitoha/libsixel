#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    filename = Object.new
    def filename.to_s
      File.expand_path('tests/data/inputs/snake_64.png', ENV.fetch('TOP_SRCDIR', Dir.pwd))
    end

    begin
      Libsixel::API.sixel_loader_load_file(loader, filename, 0)
      puts 'not ok 1 - loader accepted path-like object without to_path'
    rescue TypeError
      puts 'ok 1 - loader rejects path-like object without to_path'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader filename to_path coercion check failed'
  puts "# #{e.class}: #{e.message}"
end
