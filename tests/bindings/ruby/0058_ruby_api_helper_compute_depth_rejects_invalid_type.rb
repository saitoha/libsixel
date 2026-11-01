#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  begin
    Libsixel::Helper.compute_depth(Object.new)
    puts 'not ok 1 - helper compute_depth accepted non-convertible object'
  rescue NoMethodError, TypeError, ArgumentError, RuntimeError
    puts 'ok 1 - helper compute_depth rejects non-convertible object'
  end
rescue StandardError => e
  puts 'not ok 1 - helper compute_depth invalid-type check failed'
  puts "# #{e.class}: #{e.message}"
end
