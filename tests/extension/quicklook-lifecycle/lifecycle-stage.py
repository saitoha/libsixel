#!/usr/bin/env python3
"""An explicit private executor for order/error controls; no OS mutation."""
from pathlib import Path
import json
import sys
folder, stage = Path(sys.argv[1]), sys.argv[2]
p = folder / 'stage-calls.json'
calls = json.loads(p.read_text()) if p.exists() else []
calls.append(stage)
p.write_text(json.dumps(calls))
config = json.loads((folder / 'stage-status.json').read_text())
# The conflict is created only after the command has received control. It
# models a failed completion publication, not an actual disk-full or crash.
if config.get('recordFailure') == stage:
    (folder / 'record/journal.pending').write_text('private persistence conflict')
raise SystemExit(config.get(stage, 0))
