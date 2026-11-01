#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  rejected = false

  begin
    encoder.encode('tests/data/inputs/formats/this_file_does_not_exist.png')
  rescue RuntimeError
    rejected = true
  end

  if rejected
    encoder = nil
    GC.start
    puts 'ok 1 - encoder destructor remained stable after missing path error'
  else
    puts 'not ok 1 - encoder accepted missing input path'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder missing-path destructor safety check failed'
  puts "# #{e.class}: #{e.message}"
end
