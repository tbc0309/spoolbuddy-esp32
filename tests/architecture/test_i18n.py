import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]

def test_generated_catalog_is_reproducible_and_all_referenced_keys_exist():
    catalog=ROOT/'assets/i18n_catalog.h';before=catalog.read_bytes()
    subprocess.run([sys.executable,str(ROOT/'tools/generate_i18n.py')],check=True)
    assert catalog.read_bytes()==before
    rows=set(re.findall(r'^    \{"([\w.]+)"',before.decode(),re.M))
    for p in (ROOT/'src/ui').glob('*'):
        if p.suffix in ('.c','.h'):
            assert set(re.findall(r'\btr\("([\w.]+)"\)',p.read_text(encoding='utf-8')))<=rows
            assert not re.search(r'\btr\("[^"\n]+",',p.read_text(encoding='utf-8'))

def test_vendor_locales_match_locked_upstream_sources():
    provenance=json.loads((ROOT/'vendor/spoolbuddy/i18n/provenance.json').read_text())
    assert provenance['commit']==json.loads((ROOT/'upstream-versions.json').read_text())['bambuddy']['tested_main_commit']
    source=ROOT/'upstream/bambuddy/frontend/src'
    for name,item in provenance['files'].items():
        p=source/item['source'] if 'source' in item else source/name
        if 'source' in item:p=ROOT/'upstream/bambuddy'/item['source']
        assert provenance['normalization']=='LF'
        assert hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest()==item['sha256']

def test_default_config_does_not_force_a_language():
    example=json.loads((ROOT/'sdcard/spoolbuddy.example.json').read_text())
    assert 'language' not in example
