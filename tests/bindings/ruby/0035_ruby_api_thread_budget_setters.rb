#!/usr/bin/env ruby
# Policy: docs/cli/threading-policy.md
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  [:set_encoder_threads, :set_decoder_threads].each do |setter|
    [1, '2', 'all', ' ALL ', ' AlL '.b, :all].each do |value|
      Libsixel.public_send(setter, value)
    end
    ['invalid', 0, -1, '0', '1.5', nil].each do |value|
      begin
        Libsixel.public_send(setter, value)
        raise "thread budget API accepted invalid input"
      rescue ArgumentError
        # Each setter validates independently.
      end
    end
  end
  puts 'ok 1 - both thread budget APIs validate inputs'
rescue StandardError => e
  puts 'not ok 1 - thread budget API check failed'
  puts "# #{e.class}: #{e.message}"
end
