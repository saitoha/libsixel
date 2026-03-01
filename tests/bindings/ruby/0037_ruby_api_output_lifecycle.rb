#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  output = Output.new(write_proc: ->(_data, _priv) { nil })
  output.ref
  output.unref
  output.unref

  puts 'ok 1 - output new/ref/unref lifecycle APIs are callable'
rescue StandardError => e
  puts 'not ok 1 - output lifecycle API check failed'
  puts "# #{e.class}: #{e.message}"
end
