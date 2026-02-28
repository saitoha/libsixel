#!/usr/bin/env ruby
# frozen_string_literal: true

# Emit TAP so automake/meson can consume the result uniformly.
puts '1..1'

begin
  require 'libsixel'

  if defined?(Libsixel) && Libsixel.const_defined?(:VERSION)
    puts 'ok 1 - ruby bindings module loads from bundled gem'
  else
    puts 'not ok 1 - ruby bindings module loaded without VERSION constant'
  end
rescue StandardError => e
  puts 'not ok 1 - ruby bindings module load failed'
  puts "# #{e.class}: #{e.message}"
end
