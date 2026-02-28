#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  encoder = Encoder.new
  if encoder.is_a?(Encoder)
    puts 'ok 1 - encoder initialize returns usable encoder instance'
  else
    puts 'not ok 1 - encoder initialize did not return Encoder instance'
  end
rescue StandardError => e
  puts 'not ok 1 - encoder initialize usability check failed'
  puts "# #{e.class}: #{e.message}"
end
