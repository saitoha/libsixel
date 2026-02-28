#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  missing = '/path/which/does/not/exist.png'.b

  begin
    encoder.encode(missing)
    puts 'not ok 1 - encoder accepted missing bytes path input'
  rescue RuntimeError
    puts 'ok 1 - encoder rejects missing bytes path input'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder missing bytes path rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
