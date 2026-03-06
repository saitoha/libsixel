#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  source = "#{ENV.fetch('TOP_SRCDIR')}/tests/data/inputs/snake_64.png"
  sixel_path = "#{ENV.fetch('ARTIFACT_LOCAL_DIR')}/decode_noarg_input.six"
  png_path = "#{ENV.fetch('ARTIFACT_LOCAL_DIR')}/decode_noarg_output.png"

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, sixel_path)
  encoder.encode(source)

  decoder = Decoder.new
  decoder.setopt(Libsixel::API::SIXEL_OPTFLAG_INPUT, sixel_path)
  decoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, png_path)
  decoder.decode

  if File.size(png_path).to_i > 0
    puts 'ok 1 - decoder decode works without infile argument'
  else
    puts 'not ok 1 - decoder decode did not write output'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder decode without infile check failed'
  puts "# #{e.class}: #{e.message}"
end
