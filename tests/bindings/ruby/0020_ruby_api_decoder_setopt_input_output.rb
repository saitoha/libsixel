#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  six = File.join(ENV.fetch('TOP_SRCDIR'), 'tests/data/inputs/snake_64.six')
  png = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'decoder_setopt.png')

  decoder = Decoder.new
  decoder.setopt('i', six)
  decoder.setopt('o', png)

  puts 'ok 1 - decoder setopt accepts input and output flags'
rescue StandardError => e
  puts 'not ok 1 - decoder setopt input/output check failed'
  puts "# #{e.class}: #{e.message}"
end
