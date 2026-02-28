#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  decoder = Decoder.new
  if decoder.is_a?(Decoder)
    puts 'ok 1 - decoder initialize returns usable decoder instance'
  else
    puts 'not ok 1 - decoder initialize did not return Decoder instance'
  end
rescue StandardError => e
  puts 'not ok 1 - decoder initialize usability check failed'
  puts "# #{e.class}: #{e.message}"
end
