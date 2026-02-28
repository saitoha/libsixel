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
    status = Libsixel::API.sixel_loader_load_file(loader, '/definitely/missing/image.png'.b, 0)
    if Libsixel::API.failed?(status)
      puts 'ok 1 - loader load_file handles bytes filename on missing path'
    else
      puts 'not ok 1 - loader load_file unexpectedly succeeded on missing bytes path'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader bytes filename missing-path check failed'
  puts "# #{e.class}: #{e.message}"
end
