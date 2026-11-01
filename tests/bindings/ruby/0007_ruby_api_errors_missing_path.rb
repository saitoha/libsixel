#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.encode('/path/that/does/not/exist.png')
    puts 'not ok 1 - encoder accepted missing input path'
  rescue RuntimeError
    puts 'ok 1 - encoder rejects missing input path'
  end
rescue StandardError => e
  puts 'not ok 1 - missing input path check failed'
  puts "# #{e.class}: #{e.message}"
end
