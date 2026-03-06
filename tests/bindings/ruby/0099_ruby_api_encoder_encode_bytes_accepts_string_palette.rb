#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
Dir.mkdir(artifact_local_dir) unless File.directory?(artifact_local_dir)


begin
  require 'libsixel'

  artifact_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
  output = File.join(artifact_dir, 'encode_bytes_string_palette.six')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, output)

  pixels = [0, 1, 2, 3, 0, 1, 2, 3].pack('C*')
  palette = [
    255, 0, 0,
    0, 255, 0,
    0, 0, 255,
    255, 255, 255
  ].pack('C*')

  status = Libsixel::API.sixel_encoder_encode_bytes(
    encoder.instance_variable_get(:@ptr),
    pixels,
    4,
    2,
    Libsixel::API::SIXEL_PIXELFORMAT_PAL8,
    palette,
    palette.bytesize
  )

  if Libsixel::API.failed?(status)
    puts 'not ok 1 - encoder encode_bytes rejected string palette payload'
  else
    sixel_payload = File.binread(output)
    if sixel_payload.empty?
      puts 'not ok 1 - encoder output missing for string palette payload'
    elsif sixel_payload.start_with?("\eP") && sixel_payload.end_with?("\e\\")
      puts 'ok 1 - encoder encode_bytes accepts string palette payload'
    else
      puts 'not ok 1 - encoder output is not a valid sixel envelope'
    end
  end
rescue StandardError => e
  puts 'not ok 1 - encoder encode_bytes string palette acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end
