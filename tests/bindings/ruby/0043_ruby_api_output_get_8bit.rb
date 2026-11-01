#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  output = Output.new(write_proc: ->(_data, _priv) { nil })
  availability = output.get_8bit_availability

  if availability == 0 || availability == 1
    puts "ok 1 - output 8bit getter returned a valid state (#{availability})"
  else
    puts "not ok 1 - output 8bit getter returned invalid state: #{availability}"
  end
rescue StandardError => e
  puts 'not ok 1 - output 8bit getter check failed'
  puts "# #{e.class}: #{e.message}"
end
