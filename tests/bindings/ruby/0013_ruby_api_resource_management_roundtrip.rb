#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  src = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  six = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'resource_roundtrip.six')
  png = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'resource_roundtrip.png')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, six)
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_WIDTH, '96')
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_HEIGHT, '72')
  encoder.encode(src)

  decoder = Decoder.new
  decoder.setopt('i', six)
  decoder.setopt('o', png)
  decoder.decode

  if File.size(six).positive? && File.size(png).positive?
    puts 'ok 1 - encode/decode roundtrip writes both artifact files'
  else
    puts 'not ok 1 - roundtrip artifacts are missing or empty'
  end
rescue StandardError => e
  puts 'not ok 1 - resource management roundtrip check failed'
  puts "# #{e.class}: #{e.message}"
end
