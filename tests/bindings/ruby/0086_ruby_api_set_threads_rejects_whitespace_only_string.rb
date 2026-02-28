#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel.set_threads('   ')
    puts 'not ok 1 - set_threads accepted whitespace-only string input'
  rescue ArgumentError
    puts 'ok 1 - set_threads rejects whitespace-only string input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_threads whitespace-only string validation check failed'
  puts "# #{e.class}: #{e.message}"
end
