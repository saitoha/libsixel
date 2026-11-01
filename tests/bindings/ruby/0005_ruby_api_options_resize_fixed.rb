#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  src = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  six = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'resize_fixed.six')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, six)
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_WIDTH, '64')
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_HEIGHT, '32')
  encoder.encode(src)

  payload = File.binread(six)
  if payload.start_with?("\eP") && payload.end_with?("\e\\")
    puts 'ok 1 - resize fixed options produce valid sixel output'
  else
    puts 'not ok 1 - resize fixed options did not produce valid sixel output'
  end
rescue StandardError => e
  puts 'not ok 1 - fixed resize option check failed'
  puts "# #{e.class}: #{e.message}"
end
