#!/usr/bin/env ruby
# frozen_string_literal: true

require 'tmpdir'

puts '1..1'

begin
  require 'libsixel'

  pixels = [0, 1, 2, 3, 0, 1, 2, 3].pack('C*')
  palette = [
    255, 0, 0,
    0, 255, 0,
    0, 0, 255,
    255, 255, 255
  ]

  Dir.mktmpdir('libsixel-ruby-0096') do |tmpdir|
    output = File.join(tmpdir, 'encode_bytes_bytearray_palette.six')

    encoder = Encoder.new
    encoder.setopt('o', output)
    encoder.encode_bytes(
      pixels,
      width: 4,
      height: 2,
      pixelformat: Libsixel::API::SIXEL_PIXELFORMAT_PAL8,
      palette: palette
    )

    payload = File.binread(output)
    if payload.empty?
      puts 'not ok 1 - encoder output missing for bytearray-like palette input'
    elsif payload.start_with?("\eP") && payload.end_with?("\e\\")
      puts 'ok 1 - encoder encode_bytes accepts bytearray-like palette input'
    else
      puts 'not ok 1 - encoder output is not a valid sixel envelope'
    end
  end
rescue StandardError => e
  puts 'not ok 1 - encoder encode_bytes bytearray-like palette acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end
