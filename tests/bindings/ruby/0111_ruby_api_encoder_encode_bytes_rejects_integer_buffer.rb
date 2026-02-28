#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode_bytes(
      123,
      width: 1,
      height: 1,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888
    )
    puts 'not ok 1 - encoder encode_bytes accepted integer pixel buffer input'
  rescue TypeError
    puts 'ok 1 - encoder encode_bytes rejects integer pixel buffer input'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder encode_bytes integer buffer rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
