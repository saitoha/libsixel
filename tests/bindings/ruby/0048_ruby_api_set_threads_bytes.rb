#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  Libsixel.set_threads('2'.b)
  Libsixel.set_threads('auto'.b)

  begin
    Libsixel.set_threads('bad'.b)
    puts 'not ok 1 - set_threads accepted invalid bytes input'
  rescue ArgumentError
    puts 'ok 1 - set_threads accepts valid bytes and rejects invalid bytes'
  end
rescue StandardError => e
  puts 'not ok 1 - set_threads bytes input validation check failed'
  puts "# #{e.class}: #{e.message}"
end
