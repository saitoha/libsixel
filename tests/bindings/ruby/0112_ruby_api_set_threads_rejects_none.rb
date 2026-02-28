#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel.set_threads(nil)
    puts 'not ok 1 - set_threads accepted nil input'
  rescue ArgumentError
    puts 'ok 1 - set_threads rejects nil input'
  end
rescue StandardError => e
  puts 'not ok 1 - set_threads nil validation check failed'
  puts "# #{e.class}: #{e.message}"
end
