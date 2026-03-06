#!/usr/bin/env ruby
# frozen_string_literal: true

require 'pathname'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
Dir.mkdir(artifact_local_dir) unless File.directory?(artifact_local_dir)


begin
  require 'libsixel'

  source = Pathname.new(File.expand_path('tests/data/inputs/snake_64.png', ENV.fetch('TOP_SRCDIR', Dir.pwd)))
  artifact_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
  output = Pathname.new(File.join(artifact_dir, 'encode_pathlike.six'))

  encoder = Encoder.new
  encoder.setopt('o', output.to_s)
  encoder.encode(source)

  payload = File.binread(output)
  if payload.empty?
    puts 'not ok 1 - encoder output missing for path-like input'
  elsif payload.start_with?("\eP") && payload.end_with?("\e\\")
    puts 'ok 1 - encoder accepts path-like input'
  else
    puts 'not ok 1 - encoder output is not a valid sixel envelope'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder path-like input acceptance check failed'
  puts "# #{e.class}: #{e.message}"
end
