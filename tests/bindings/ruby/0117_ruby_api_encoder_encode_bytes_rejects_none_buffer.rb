#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode_bytes(
      nil,
      width: 1,
      height: 1,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888
    )
    puts 'not ok 1 - encoder encode_bytes accepted nil buffer input'
  rescue ArgumentError, TypeError, RuntimeError
    puts 'ok 1 - encoder encode_bytes rejects nil buffer input'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder encode_bytes nil buffer rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
