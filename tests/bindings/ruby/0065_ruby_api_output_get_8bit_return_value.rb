#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  output = Output.new(write_proc: ->(_data, _priv) { nil })
  output.set_8bit_availability(1)
  result = output.get_8bit_availability

  if result == 0 || result == 1
    puts "ok 1 - output get_8bit returned valid state (#{result})"
  else
    puts "not ok 1 - output get_8bit returned invalid state: #{result}"
  end
rescue StandardError => e
  puts 'not ok 1 - output get_8bit return value check failed'
  puts "# #{e.class}: #{e.message}"
end
