"""Build a minimal inventory with three independent sources of membership."""

from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(sys.argv[1]) / 'tests/_static/python'))
from test_plan_links import inventory_errors


class Fixture:
    member = 'tests/owner/behavior/0001_observation.t'

    def __enter__(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='libsixel-plan-')
        self.root = Path(self.temporary.name)
        self.write(self.member, '')
        self.write('tests/_support/test-plans/one.list', self.member + '\n')
        self.write('docs/plan.md',
            '<!-- test-plan: @tests/_support/test-plans/one.list -->\n'
            f'[{self.member}](../{self.member})\n<!-- test-plan-end -->\n')
        self.write('references/test-plan-links', 'docs/plan.md|' + self.member + '\n')
        self.write('references/test-plan-errors', '')
        return self

    def __exit__(self, *args):
        self.temporary.cleanup()

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)

    def errors(self):
        return inventory_errors(self.root, self.root / 'references')
