#!/usr/bin/env python3
import os,sys,json,plistlib
from pathlib import Path
from guard_probe import observe
root=Path(os.environ['QUICKLOOK_PRIVATE_ROOT']);receipt=root.parent/'receipt.json';marker=root.parent/'final-check-marker'
assert root != Path('/') and (root.parent/'private-guard-marker').is_file()
if os.environ.get('QL_CLEANUP_FAILURE'):
 (root/'private/var/run/libsixel-quicklook.lock/foreign').write_text('retain')
if sys.argv[1:]==['--pkgs-plist']:
 sys.stdout.buffer.write(plistlib.dumps(['com.saitoha.libsixel.quicklook.pkg'] if receipt.exists() else []))
elif sys.argv[1:]==['--pkg-info-plist','com.saitoha.libsixel.quicklook.pkg']:
 if marker.exists() and os.environ.get('QL_PROBE'):
  marker.unlink();observe('installer-returned-final-receipt-observation')
 sys.stdout.buffer.write(plistlib.dumps(json.loads(receipt.read_text())))
elif sys.argv[1:]==['--forget','com.saitoha.libsixel.quicklook.pkg']:
 if os.environ.get('QL_FORGET'):sys.exit(int(os.environ['QL_FORGET']))
 receipt.unlink()
else:sys.exit(91)
