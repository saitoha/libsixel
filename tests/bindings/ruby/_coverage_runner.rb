#!/usr/bin/env ruby
# frozen_string_literal: true

require 'coverage'
require 'fileutils'

if ARGV.length != 2
  warn "usage: #{$PROGRAM_NAME} <test-script> <coverage-marshal>"
  exit 2
end

test_script = ARGV[0]
coverage_marshal = ARGV[1]

Coverage.start
status = 0

begin
  load test_script
rescue SystemExit => e
  status = e.status.nil? ? 1 : e.status.to_i
rescue Exception => e # rubocop:disable Lint/RescueException
  warn "#{e.class}: #{e.message}"
  warn e.backtrace.join("\n") if e.backtrace
  status = 1
ensure
  begin
    FileUtils.mkdir_p(File.dirname(coverage_marshal))
    File.binwrite(coverage_marshal, Marshal.dump(Coverage.result))
  rescue Exception => e # rubocop:disable Lint/RescueException
    warn "coverage dump failed: #{e.class}: #{e.message}"
    status = 1 if status.zero?
  end
end

exit status
