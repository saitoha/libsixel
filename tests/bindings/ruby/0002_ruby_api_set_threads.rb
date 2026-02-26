#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  Libsixel.set_threads(1)
  Libsixel.set_threads('auto')

  begin
    Libsixel.set_threads('invalid')
    puts 'not ok 1 - set_threads accepted invalid input'
  rescue ArgumentError
    puts 'ok 1 - set_threads accepts valid inputs and rejects invalid input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_threads API check failed'
  puts "# #{e.class}: #{e.message}"
end
