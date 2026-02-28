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
    begin
      Libsixel::API.sixel_loader_setopt(loader, 'loader-order', 'stb,png')
      puts 'not ok 1 - loader accepted non-numeric option identifier'
    rescue TypeError, ArgumentError
      puts 'ok 1 - loader setopt rejects non-numeric option identifier'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader non-numeric option identifier rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
