#!/usr/bin/env ruby
# frozen_string_literal: true

require 'pathname'

puts '1..1'

begin
  require 'libsixel'

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    status = Libsixel::API.sixel_loader_setopt(
      loader,
      Libsixel::API::SIXEL_LOADER_OPTION_LOADER_ORDER,
      Pathname.new('builtin')
    )

    if Libsixel::API.failed?(status)
      puts 'not ok 1 - loader setopt rejected path-like value coercion'
    else
      puts 'ok 1 - loader setopt accepts path-like value coercion'
    end
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue TypeError => e
  puts 'not ok 1 - loader setopt path-like value coercion raised TypeError'
  puts "# #{e.class}: #{e.message}"
rescue StandardError => e
  puts 'not ok 1 - loader setopt path-like coercion check failed'
  puts "# #{e.class}: #{e.message}"
end
