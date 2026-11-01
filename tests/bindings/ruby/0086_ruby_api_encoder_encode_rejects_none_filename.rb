#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new

  begin
    encoder.encode(nil)
    puts 'not ok 1 - encoder accepted nil filename input'
  rescue RuntimeError, TypeError, ArgumentError
    puts 'ok 1 - encoder rejects nil filename input'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder nil filename rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
