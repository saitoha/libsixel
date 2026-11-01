#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'

puts '1..1'
artifact_local_dir = ENV.fetch('ARTIFACT_LOCAL_DIR')
FileUtils.mkdir_p(artifact_local_dir)


begin
  require 'libsixel'

  src = '/dev/null'
  out = File.join(ENV.fetch('ARTIFACT_LOCAL_DIR'), 'corrupted_input.six')
  encoder = Encoder.new
  encoder.setopt(Libsixel::API::SIXEL_OPTFLAG_OUTPUT, out)

  begin
    encoder.encode(src)
    payload = File.exist?(out) ? File.binread(out) : ''
    if payload.empty? || (payload.start_with?("\eP") && payload.end_with?("\e\\"))
      puts 'ok 1 - encoder handles empty/corrupted-style input without crashing'
    else
      puts 'not ok 1 - encoder produced malformed output for empty input'
    end
  rescue RuntimeError
    puts 'ok 1 - encoder rejects empty/corrupted-style input'
  end
rescue StandardError => e
  puts 'not ok 1 - corrupted image handling check failed'
  puts "# #{e.class}: #{e.message}"
end
