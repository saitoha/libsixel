"""Exercise the production classifier against a small physical source tree."""

from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(sys.argv[1]) / 'tests/_static/python'))
from test_layout import inspect


class Fixture:
    def __enter__(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='libsixel-layout-')
        self.root = Path(self.temporary.name)
        return self

    def __exit__(self, *args):
        self.temporary.cleanup()

    def add(self, name):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
        return path

    def cases(self, count, leaf='owner/behavior'):
        for number in range(1, count + 1):
            self.add(f'{leaf}/{number:04d}_observation.t')

    def errors(self):
        return inspect(self.root)[2]
