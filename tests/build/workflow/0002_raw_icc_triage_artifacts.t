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
require 'yaml'

root = ARGV.fetch(0)
workflow = YAML.load_file(File.join(root, '.github/workflows/triage.yml'))
steps = workflow.fetch('jobs').fetch('fuzzing').fetch('steps')
upload = steps.find { |step| step['name'] == 'Upload fuzz replay artifacts' }
abort 'triage artifact upload is missing' unless upload
inputs = upload.fetch('with')
replay = File.read(File.join(root, '.github/scripts/replay-cms-builtin.sh'))
directory = replay[/^artifact_dir="\$PWD\/([^"]+)"$/, 1]
abort 'raw ICC replay artifact directory is missing' unless directory

# Directory matches are recursive in upload-artifact, but a wildcard
# rooted under .tmp requires opting in to hidden files.
includes_replay = inputs.fetch('path').lines.any? do |pattern|
  File.fnmatch?(pattern.strip, directory, File::FNM_PATHNAME)
end
unless upload.fetch('uses').start_with?('actions/upload-artifact@') &&
    upload['if'] == 'always()' && includes_replay &&
    inputs['include-hidden-files'].to_s == 'true'
  abort "triage upload excludes #{directory}/replay.log"
end
RUBY
    echo "not ok 1 - raw ICC triage uploads its replay artifacts"
    exit 0
}

echo "ok 1 - raw ICC triage uploads its replay artifacts"
exit 0
