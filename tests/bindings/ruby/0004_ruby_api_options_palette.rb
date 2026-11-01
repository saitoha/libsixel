#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  src = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  out = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'options_palette.six')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, out)
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_COLORS, '16')
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_DIFFUSION, 'atkinson')
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_PALETTE_TYPE, 'hls')
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_QUALITY, 'high')
  encoder.encode(src)

  payload = File.binread(out)
  if payload.start_with?("\eP") && payload.end_with?("\e\\")
    puts 'ok 1 - encoder palette-related options keep output valid'
  else
    puts 'not ok 1 - encoder output is not wrapped as valid sixel'
  end
rescue StandardError => e
  puts 'not ok 1 - palette option encode check failed'
  puts "# #{e.class}: #{e.message}"
end
