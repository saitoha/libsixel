#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  to_str_value = Object.new
  def to_str_value.to_str
    'stb,png'
  end

  out = Libsixel::API::Util.make_outptr
  status = Libsixel::API.sixel_loader_new(out, 0)
  raise RuntimeError, "sixel_loader_new failed: #{status}" if Libsixel::API.failed?(status)

  loader = Libsixel::API::Util.read_outptr(out)
  begin
    status = Libsixel::API.sixel_loader_setopt(
      loader,
      Libsixel::API::SIXEL_LOADER_OPTION_LOADER_ORDER,
      to_str_value
    )

    if Libsixel::API.failed?(status)
      puts 'not ok 1 - loader setopt rejected to_str-coercible value'
    else
      puts 'ok 1 - loader setopt accepts to_str-coercible value'
    end
  rescue TypeError => e
    puts 'not ok 1 - loader setopt to_str-coercible value raised TypeError'
    puts "# #{e.class}: #{e.message}"
  ensure
    Libsixel::API.sixel_loader_unref(loader) if loader && loader.to_i != 0
  end
rescue StandardError => e
  puts 'not ok 1 - loader setopt to_str coercion check failed'
  puts "# #{e.class}: #{e.message}"
end
