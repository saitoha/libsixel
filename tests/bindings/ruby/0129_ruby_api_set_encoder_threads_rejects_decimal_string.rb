#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel.set_encoder_threads('1.5')
    puts 'not ok 1 - set_encoder_threads accepted decimal numeric string input'
  rescue ArgumentError
    puts 'ok 1 - set_encoder_threads rejects decimal numeric string input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_encoder_threads decimal string validation check failed'
  puts "# #{e.class}: #{e.message}"
end
