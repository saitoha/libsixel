#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  src = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  six = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'decoder_decode_input.six')
  png = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'decoder_decode_output.png')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, six)
  encoder.encode(src)

  decoder = Decoder.new
  decoder.setopt('i', six)
  decoder.setopt('o', png)
  decoder.decode

  if File.size(png).positive?
    puts 'ok 1 - decoder decode writes non-empty png output'
  else
    puts 'not ok 1 - decoder decode output is empty'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder decode output check failed'
  puts "# #{e.class}: #{e.message}"
end
