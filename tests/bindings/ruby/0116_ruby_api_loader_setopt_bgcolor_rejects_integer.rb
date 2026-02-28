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
      status = Libsixel::API.sixel_loader_setopt(
        loader,
        Libsixel::API::SIXEL_LOADER_OPTION_BGCOLOR,
        1
      )
      if Libsixel::API.failed?(status)
        puts 'ok 1 - loader setopt rejects integer bgcolor input'
      else
        puts 'not ok 1 - loader setopt accepted integer bgcolor input'
      end
    rescue TypeError, ArgumentError
      puts 'ok 1 - loader setopt rejects integer bgcolor input'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader integer bgcolor rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
