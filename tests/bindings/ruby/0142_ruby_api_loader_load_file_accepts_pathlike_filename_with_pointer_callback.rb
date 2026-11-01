#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fiddle'
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
    status = Libsixel::API.sixel_loader_load_file(loader, source, Fiddle::Pointer.new(0))
    if Libsixel::API.failed?(status)
      puts 'ok 1 - loader load_file accepts path-like filename and pointer callback types'
    else
      puts 'ok 1 - loader load_file accepts path-like filename and pointer callback types'
    end
  rescue TypeError => e
    puts 'not ok 1 - loader wrapper rejected path-like filename or pointer callback'
    puts "# #{e.class}: #{e.message}"
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader path-like/pointer callback acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end
