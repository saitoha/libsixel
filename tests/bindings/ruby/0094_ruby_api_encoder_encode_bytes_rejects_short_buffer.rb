#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode_bytes(
      [255, 0, 0].pack('C*'),
      width: 2,
      height: 2,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888
    )
    puts 'not ok 1 - encoder encode_bytes accepted too-short pixel buffer'
  rescue RuntimeError, ArgumentError, TypeError
    puts 'ok 1 - encoder encode_bytes rejects too-short pixel buffer'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder short-buffer rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
