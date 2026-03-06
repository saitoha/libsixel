#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  src = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.png')
  out = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'format_png.six')

  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, out)
  encoder.encode(src)

  payload = File.binread(out)
  if payload.start_with?("\ePq") && payload.rstrip.end_with?("\e\\")
    puts 'ok 1 - encoder writes sixel envelope for png input'
  else
    puts 'not ok 1 - encoded payload does not contain valid sixel envelope'
  end
rescue StandardError => e
  puts 'not ok 1 - png encode envelope check failed'
  puts "# #{e.class}: #{e.message}"
end
