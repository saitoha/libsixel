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
      status = Libsixel::API.sixel_loader_load_file(loader, true, 0)
      if Libsixel::API.failed?(status)
        puts 'ok 1 - loader true filename rejection path verified'
      else
        puts 'not ok 1 - loader accepted true filename unexpectedly'
      end
    rescue TypeError
      puts 'ok 1 - loader true filename type rejection verified'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader true filename rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
