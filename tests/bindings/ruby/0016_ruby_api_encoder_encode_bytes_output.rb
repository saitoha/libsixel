#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  out = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'encoder_encode_bytes.six')
  pixels = [255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255].pack('C*')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, out)
  encoder.encode_bytes(
    pixels,
    width: 2,
    height: 2,
    pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_RGB888
  )

  if File.size(out).positive?
    puts 'ok 1 - encoder encode_bytes writes non-empty sixel output'
  else
    puts 'not ok 1 - encoder encode_bytes output is empty'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder encode_bytes output check failed'
  puts "# #{e.class}: #{e.message}"
end
