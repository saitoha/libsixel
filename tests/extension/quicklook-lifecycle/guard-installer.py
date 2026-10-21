#!/usr/bin/env python3
import os,sys,subprocess,shutil,json
from pathlib import Path
from guard_probe import observe
root=Path(os.environ['QUICKLOOK_PRIVATE_ROOT']);pkg=Path(sys.argv[2]).parent;manifest=pkg/'payload-manifest.json';tool=Path(os.environ['QUICKLOOK_PRIVATE_DRIVER'])
assert root != Path('/') and (root.parent/'private-guard-marker').is_file()
if os.environ.get('QL_BEFORE_PRE'):sys.exit(int(os.environ['QL_BEFORE_PRE']))
assert sys.argv[1:2]==['-pkg'] and sys.argv[3:]==['-target','/']
r=subprocess.run([tool,'begin-install','/',manifest]);
if r.returncode:sys.exit(r.returncode)
for rel in ['Applications/SixelQuickLookHost.app','Applications/SixelPreviewBridge.app','Library/Application Support/libsixel/QuickLook']:
 target=root/rel
 if target.exists():shutil.rmtree(target)
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(pkg/'work/expanded/payload.pkg/Payload'/rel,target)
if os.environ.get('QL_BEFORE_POST'):sys.exit(int(os.environ['QL_BEFORE_POST']))
r=subprocess.run([tool,'end-install','/',manifest]);
if r.returncode:sys.exit(r.returncode)
if os.environ.get('QL_PROBE'):observe('postinstall-before-receipt')
if os.environ.get('QL_AFTER_POST'):sys.exit(int(os.environ['QL_AFTER_POST']))
(root.parent/'receipt.json').write_text(json.dumps({'pkgid':'com.saitoha.libsixel.quicklook.pkg','pkg-version':json.loads(manifest.read_text())['packageVersion'],'volume':'/','install-location':'/'}))
if os.environ.get('QL_BAD_RECEIPT'):
 data=json.loads((root.parent/'receipt.json').read_text());data['pkg-version']='9.0.0';(root.parent/'receipt.json').write_text(json.dumps(data))
if os.environ.get('QL_PROBE'):
 observe('receipt-committed-before-installer-return')
 (root.parent/'final-check-marker').write_text('probe at final receipt read')
