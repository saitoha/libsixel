#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
Dir.mkdir(artifact_local_dir) unless File.directory?(artifact_local_dir)


begin
  require 'libsixel'

  src = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  out = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'encoder_encode.six')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, out)
  encoder.encode(src)

  if File.size(out).positive?
    puts 'ok 1 - encoder encode writes non-empty sixel output'
  else
    puts 'not ok 1 - encoder output is empty'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder encode output check failed'
  puts "# #{e.class}: #{e.message}"
end
