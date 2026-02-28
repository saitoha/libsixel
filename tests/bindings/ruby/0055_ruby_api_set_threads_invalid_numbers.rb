#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  rejected_zero = false
  rejected_negative = false

  begin
    Libsixel.set_threads(0)
  rescue ArgumentError
    rejected_zero = true
  end

  begin
    Libsixel.set_threads(-1)
  rescue ArgumentError
    rejected_negative = true
  end

  if rejected_zero && rejected_negative
    puts 'ok 1 - set_threads rejects zero and negative numeric inputs'
  else
    puts 'not ok 1 - set_threads accepted non-positive numeric input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_threads numeric validation check failed'
  puts "# #{e.class}: #{e.message}"
end
