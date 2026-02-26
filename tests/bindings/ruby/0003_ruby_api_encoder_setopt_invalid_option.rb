#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  begin
    encoder.setopt('X', '16')
    puts 'not ok 1 - encoder accepted unsupported option flag'
  rescue RuntimeError
    puts 'ok 1 - encoder rejects unsupported option flag'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder option validation check failed'
  puts "# #{e.class}: #{e.message}"
end
