#!/usr/bin/env ruby
# frozen_string_literal: true

# Build a bundled libsixel Ruby platform gem from an existing shared library.
#
# Workflow:
# 1. Resolve libsixel shared library from --libpath or --libdir.
# 2. Copy the library into ruby/lib/libsixel/_libs.
# 3. Run `gem build` with an optional platform override.
# 4. Remove the temporary copy after the gem is created.

require 'fileutils'
require 'optparse'
require 'pathname'
require 'rubygems'

SIXEL_NAMES = %w[sixel libsixel sixel-1 libsixel-1 msys-sixel cygsixel].freeze
SIXEL_SUFFIXES = %w[.so .dylib .dll].freeze


def find_library_in_dir(libdir)
  candidates = []
  SIXEL_NAMES.each do |name|
    ['', 'lib'].each do |prefix|
      SIXEL_SUFFIXES.each do |suffix|
        candidates.concat(Dir.glob(File.join(libdir, "#{prefix}#{name}*#{suffix}")))
      end
    end
  end

  candidates.sort.each do |path|
    next if path.end_with?('.dll.a', '.dll.def', '.lib')

    return path
  end

  nil
end




def normalize_gem_version(raw)
  version = raw.to_s.strip
  raise ArgumentError, 'gem version is empty' if version.empty?

  # RubyGems versions must be dot-separated alnum segments.
  version = version.gsub(/[^0-9A-Za-z.]+/, '.')
  version = version.gsub(/\.+/, '.')
  version = version.sub(/\A\./, '')
  version = version.sub(/\.\z/, '')
  version = '0.0.0' if version.empty?
  unless version.match?(/\A\d/)
    version = '0.0.0.' + version
  end

  version
end

def main
  options = {
    distdir: nil,
    libpath: nil,
    libdir: nil,
    platform: nil,
    version: nil
  }

  OptionParser.new do |parser|
    parser.banner = 'Usage: gem_builder.rb --distdir DIR [--libpath FILE | --libdir DIR]'
    parser.on('--distdir DIR', 'Output directory for the generated gem') { |v| options[:distdir] = v }
    parser.on('--libpath FILE', 'Path to the built libsixel shared library') { |v| options[:libpath] = v }
    parser.on('--libdir DIR', 'Directory to scan for libsixel shared libraries') { |v| options[:libdir] = v }
    parser.on('--platform NAME', 'Gem platform override (e.g. x86_64-linux)') { |v| options[:platform] = v }
    parser.on('--version VERSION', 'Gem version override (normalized for RubyGems)') { |v| options[:version] = v }
  end.parse!

  raise ArgumentError, '--distdir is required' if options[:distdir].nil?
  if options[:libpath].nil? && options[:libdir].nil?
    raise ArgumentError, 'Either --libpath or --libdir must be specified'
  end

  libpath = options[:libpath]
  libpath = find_library_in_dir(options[:libdir]) if libpath.nil?
  raise "libsixel shared library was not found" if libpath.nil?

  root = Pathname.new(__dir__).expand_path
  distdir = Pathname.new(options[:distdir]).expand_path
  libs_dir = root.join('lib', 'libsixel', '_libs')
  copied = libs_dir.join(File.basename(libpath))

  FileUtils.mkdir_p(distdir)
  FileUtils.mkdir_p(libs_dir)
  FileUtils.cp(libpath, copied)

  env = {}
  # Always emit a platform gem for build-system generated artifacts so
  # runtime loaders can rely on bundled shared objects living under
  # lib/libsixel/_libs. Callers may still override with --platform for
  # cross packaging jobs.
  platform = options[:platform]
  platform = Gem::Platform.local.to_s if platform.nil? || platform.empty?
  env['LIBSIXEL_RUBY_GEM_PLATFORM'] = platform

  source_version = options[:version]
  source_version = ENV['LIBSIXEL_RUBY_GEM_VERSION'] if source_version.nil?
  if source_version.nil?
    source_version = File.read(root.join('lib', 'libsixel', 'version.rb')).match(/VERSION\s*=\s*['"]([^'"]+)['"]/)[1]
  end

  version = normalize_gem_version(source_version)
  env['LIBSIXEL_RUBY_GEM_VERSION'] = version

  output = distdir.join("libsixel-ruby-#{version}-#{platform}.gem")

  begin
    ok = system(env, 'gem', 'build', 'libsixel-ruby.gemspec', '--output', output.to_s, chdir: root.to_s)
    raise 'gem build failed' unless ok
  ensure
    FileUtils.rm_f(copied)
    begin
      Dir.rmdir(libs_dir)
    rescue SystemCallError
      nil
    end
  end

  0
end

begin
  exit(main)
rescue StandardError => e
  warn("gem_builder.rb: #{e.message}")
  exit(1)
end
