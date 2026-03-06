#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
Dir.mkdir(artifact_local_dir) unless File.directory?(artifact_local_dir)


begin
  require 'libsixel'

  src = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  out = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'encoder_setopt.six')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_INPUT, src)
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, out)

  puts 'ok 1 - encoder setopt accepts input and output flags'
rescue StandardError => e
  puts 'not ok 1 - encoder setopt input/output check failed'
  puts "# #{e.class}: #{e.message}"
end
