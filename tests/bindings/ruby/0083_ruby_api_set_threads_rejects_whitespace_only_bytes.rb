#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel.set_threads('   '.b)
    puts 'not ok 1 - set_threads accepted whitespace-only bytes input'
  rescue ArgumentError
    puts 'ok 1 - set_threads rejects whitespace-only bytes input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_threads whitespace-only bytes rejection check failed'
  puts "# #{e.class}: #{e.message}"
end
