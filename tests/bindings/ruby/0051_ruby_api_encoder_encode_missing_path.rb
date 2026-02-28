#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode('tests/data/inputs/formats/this_file_does_not_exist.png')
    puts 'not ok 1 - encoder accepted non-existing input file'
  rescue RuntimeError
    puts 'ok 1 - encoder rejects non-existing input file'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder missing file check failed'
  puts "# #{e.class}: #{e.message}"
end
