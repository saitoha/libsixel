#!/usr/bin/env python3
"""Keep standalone Windows probes out of the aggregate runner source list."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(sys.argv[1]) / 'tests/_static/python'))
from source_files import excluded_path

assert excluded_path(r'C:\repo\tests\extension\wic-installed\probe.c',
                     ['*/extension/wic-installed/*.c'])
