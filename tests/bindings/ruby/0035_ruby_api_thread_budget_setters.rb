#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  [:set_encoder_threads, :set_decoder_threads].each do |setter|
    Libsixel.public_send(setter, 1)
    Libsixel.public_send(setter, 'auto')
    Libsixel.public_send(setter, 'all')
    begin
      Libsixel.public_send(setter, 'invalid')
      raise "thread budget API accepted invalid input"
    rescue ArgumentError
      # Each setter validates independently.
    end
  end
  puts 'ok 1 - both thread budget APIs validate inputs'
rescue StandardError => e
  puts 'not ok 1 - thread budget API check failed'
  puts "# #{e.class}: #{e.message}"
end
