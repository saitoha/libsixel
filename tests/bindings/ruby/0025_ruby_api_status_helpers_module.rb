#!/usr/bin/env ruby
# frozen_string_literal: true

puts '1..1'

begin
  require 'libsixel'

  statuses = [
    [Libsixel::API::SIXEL_FALSE, false],
    [Libsixel::API::SIXEL_COM_ERROR, false],
    [Libsixel::API::SIXEL_RUNTIME_ERROR, false]
  ]

  valid = statuses.all? do |status, expected_success|
    succeeded = Libsixel::API.succeeded?(status)
    failed = Libsixel::API.failed?(status)
    succeeded != failed && succeeded == expected_success && failed != expected_success
  end

  if valid
    puts 'ok 1 - status helper predicates classify success/failure consistently'
  else
    puts 'not ok 1 - status helper predicates returned contradictory results'
  end
rescue StandardError => e
  puts 'not ok 1 - status helper predicate check failed'
  puts "# #{e.class}: #{e.message}"
end
