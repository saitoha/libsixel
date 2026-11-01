#!/usr/bin/env python3
"""Check one command-dependency boundary without executing the specimen."""
# Policy: docs/testing/guide.md

from pathlib import Path
import sys

sys.path.insert(0, str(Path(sys.argv[1]) / 'tests/_static/python'))
from no_find import contains_find

assert contains_find('env LC_ALL=C /usr/bin/find .') is True
