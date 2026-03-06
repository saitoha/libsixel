#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  source = File.expand_path('tests/data/inputs/snake_64.png', ENV.fetch('TOP_SRCDIR', Dir.pwd)).dup
  source.force_encoding(Encoding::ASCII_8BIT)
  artifact_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
  output = File.join(artifact_dir, 'encode_bytes_filename.six')

  encoder = Encoder.new
  encoder.setopt('o', output)
  encoder.encode(source)

  if File.size(output).positive?
    puts 'ok 1 - encoder accepts bytes filename input'
  else
    puts 'not ok 1 - encoder output missing for bytes filename input'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder bytes filename acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end
