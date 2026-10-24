#!/usr/bin/env ruby
# Policy: docs/cli/threading-policy.md
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  [:set_encoder_threads, :set_decoder_threads].each do |setter|
    ['auto', 'AUTO', ' AuTo ', 'auto'.b, :auto].each do |value|
      begin
        Libsixel.public_send(setter, value)
        raise 'thread budget API accepted auto'
      rescue ArgumentError => e
        raise unless e.message == "threads must be a positive integer or 'all'"
      end
    end
  end
  puts 'ok 1 - both thread budget setters reject auto'
rescue StandardError => e
  puts 'not ok 1 - thread budget auto rejection failed'
  puts "# #{e.class}: #{e.message}"
end
