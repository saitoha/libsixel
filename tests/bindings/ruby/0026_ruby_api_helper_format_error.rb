#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  status = if Libsixel::API.const_defined?(:SIXEL_RUNTIME_ERROR)
             Libsixel::API::SIXEL_RUNTIME_ERROR
           else
             4352
           end
  message = Libsixel::Helper.format_error(status)

  if message.nil? || message.empty?
    puts 'not ok 1 - helper format_error returned an empty message'
  else
    puts 'ok 1 - helper format_error returned a non-empty message'
  end
rescue StandardError => e
  puts 'not ok 1 - helper format_error check failed'
  puts "# #{e.class}: #{e.message}"
end
