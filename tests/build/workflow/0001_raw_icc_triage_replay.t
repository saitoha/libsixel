#!/bin/sh

set -eux

echo "1..1"
set -v

test -d "${TOP_SRCDIR}/.github/workflows" || {
    echo "ok 1 # SKIP workflow checkout required"
    exit 0
}
command -v ruby >/dev/null || {
    echo "ok 1 # SKIP Ruby required"
    exit 0
}

ruby - "${TOP_SRCDIR}" <<'RUBY' || {
require 'json'
require 'open3'
require 'rbconfig'
require 'tempfile'
require 'yaml'

root = File.expand_path(ARGV.fetch(0))
workflow = YAML.load_file(File.join(root, '.github/workflows/triage.yml'))
jobs = workflow.fetch('jobs')
step = jobs.fetch('collect').fetch('steps').find { |item| item['id'] == 'collect' }
collector = step.fetch('run')[/ruby <<'RUBY'\n(.*?)^RUBY$/m, 1]
abort 'triage collector not found' unless collector

labels = %w[
  Autotools-ubuntu-x86_64-clang-fuzz-cms-builtin
  Meson-ubuntu-x86_64-clang-fuzz-cms-builtin
]
sha = 'a' * 40
Tempfile.create('libsixel-triage') do |output|
  env = {
    'WORKFLOW_FILE' => 'fuzzing.yml',
    'FAILED_JOBS_JSON' => JSON.generate(labels + ['unrelated job']),
    'MAX_JOBS' => '8',
    'RUN_META_JSON' => JSON.generate(event: 'push', head_sha: sha, run_attempt: 1),
    'CHECKOUT_REF' => 'develop',
    'CHECKOUT_SHA' => sha,
    'SOURCE_HEAD_BRANCH' => 'develop',
    'RUN_ID' => '123',
    'TRIAGE_MODE' => 'normal',
    'GITHUB_OUTPUT' => output.path
  }
  stdout, stderr, status = Open3.capture3(env, RbConfig.ruby,
    stdin_data: collector, chdir: root)
  abort "#{stdout}#{stderr}" unless status.success?
  actual = File.readlines(output.path, chomp: true).to_h { |line| line.split('=', 2) }
  entries = JSON.parse(actual.fetch('matrix')).fetch('include')
  expected = labels.zip(%w[autotools meson]).map do |label, tool|
    [label, tool, 'cms_builtin', 'cms-builtin', 'ubuntu-26.04', sha, '']
  end
  fields = %w[label buildtool kind target runner checkout_sha cache_path]
  unless entries.map { |entry| entry.values_at(*fields) } == expected &&
      actual['fuzz_labels'] == "|#{labels.join('|')}|" && actual['count'] == '2'
    abort "unexpected raw ICC replay selection: #{actual.inspect}"
  end
end

replay = jobs.fetch('fuzzing').fetch('steps')
unless replay.any? { |item| item['name'] == 'Install Ubuntu libFuzzer dependencies' &&
    item.fetch('if').include?("matrix.kind == 'cms_builtin'") } &&
    replay.any? { |item| item['if'] == "${{ matrix.kind == 'cms_builtin' }}" &&
      item.fetch('run').include?('replay-cms-builtin.sh') }
  abort 'raw ICC replay is missing its dependencies or command'
end
RUBY
    echo "not ok 1 - raw ICC triage selects both build systems and routes replay"
    exit 0
}

echo "ok 1 - raw ICC triage selects both build systems and routes replay"
exit 0
