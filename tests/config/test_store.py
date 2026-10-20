"""Persistence, backup, failed import and FAT timestamp rules using production C."""
import json
import os
import subprocess
from pathlib import Path
import pytest

ROOT=Path(__file__).resolve().parents[2]
@pytest.fixture(scope='module')
def exe(tmp_path_factory):
    work=tmp_path_factory.mktemp('config-native');binary=work/'store.exe'
    cjson=ROOT/'managed_components/espressif__cjson/cJSON'
    sources=['tests/config/store_runner.c','src/core/settings.c','src/core/config_record.c','src/platform/esp_idf/settings_store_esp.c']
    subprocess.run([str(ROOT/'.tools/sim-venv/Lib/site-packages/ziglang/zig.exe'),'cc','-O1','-target','x86_64-windows-gnu',
        '-fno-sanitize=undefined','-I'+str(ROOT/'tests/config/stubs'),'-I'+str(ROOT/'src/core'),
        '-I'+str(ROOT/'src/platform/esp_idf'),'-I'+str(cjson),*[str(ROOT/x) for x in sources],str(cjson/'cJSON.c'),'-o',str(binary)],check=True,timeout=120)
    return binary

def config(ssid='first',**extra):
    return {'wifi':{'ssid':ssid,'password':''},'bambuddy':{'url':'http://fixture.invalid:8000','api_key':''},**extra}
def card(work,data,modified):
    p=work/'spoolbuddy.json';p.write_text(json.dumps(data),encoding='utf-8');os.utime(p,(modified,modified))
def boot(exe,work,action='load',sd=True):
    result=subprocess.run([str(exe),action,'sd' if sd else 'no-sd'],cwd=work,text=True,capture_output=True,check=True,timeout=10)
    return json.loads(result.stdout)
def records(work):return [json.loads(p.read_text(encoding='utf-8')) for p in work.glob('nvs_config*.json')]

def test_first_config_saved_only_after_success_and_boots_without_card(exe,tmp_path):
    card(tmp_path,config(),1700000000)
    assert boot(exe,tmp_path)['ssid']=='first' and not records(tmp_path)
    assert boot(exe,tmp_path,'confirm')['result']
    assert boot(exe,tmp_path,sd=False)['ssid']=='first'
    assert records(tmp_path)[0]['generation']==1

def test_newer_update_retains_previous_valid_configuration(exe,tmp_path):
    card(tmp_path,config(),1700000000);boot(exe,tmp_path,'confirm')
    card(tmp_path,config('second',language='zh-TW'),1700000100)
    assert boot(exe,tmp_path)['ssid']=='second'
    assert boot(exe,tmp_path,sd=False)['ssid']=='first'
    assert boot(exe,tmp_path,'confirm')['result']
    assert boot(exe,tmp_path,sd=False)['ssid']=='second'
    assert {r['configuration']['wifi']['ssid'] for r in records(tmp_path)}=={'first','second'}
    assert boot(exe,tmp_path,sd=False)['language_override']

def test_older_or_identical_card_does_not_overwrite_or_wear_flash(exe,tmp_path):
    card(tmp_path,config(),1700000100);boot(exe,tmp_path,'confirm')
    before=records(tmp_path)
    card(tmp_path,config('old'),1700000000)
    assert boot(exe,tmp_path,'confirm')['ssid']=='first'
    card(tmp_path,config(),1700000200);boot(exe,tmp_path,'confirm')
    assert records(tmp_path)==before

def test_fat_same_timestamp_content_change_is_detected(exe,tmp_path):
    card(tmp_path,config(),1700000000);boot(exe,tmp_path,'confirm')
    card(tmp_path,config('same-second'),1700000000)
    assert boot(exe,tmp_path,'confirm')['ssid']=='same-second'
    assert max(r['generation'] for r in records(tmp_path))==2

def test_bad_card_and_failed_save_preserve_confirmed_config(exe,tmp_path):
    card(tmp_path,config(),1700000000);boot(exe,tmp_path,'confirm')
    (tmp_path/'spoolbuddy.json').write_text('{bad',encoding='utf-8')
    assert boot(exe,tmp_path)['ssid']=='first'
    card(tmp_path,config('new'),1700000100)
    assert not boot(exe,tmp_path,'fail-confirm')['result']
    assert boot(exe,tmp_path,sd=False)['ssid']=='first'

def test_corrupt_latest_bank_falls_back_to_backup(exe,tmp_path):
    card(tmp_path,config(),1700000000);boot(exe,tmp_path,'confirm')
    card(tmp_path,config('second'),1700000100);boot(exe,tmp_path,'confirm')
    latest=next(p for p in tmp_path.glob('nvs_config*.json') if json.loads(p.read_text())['generation']==2)
    latest.write_text('{power-failure',encoding='utf-8')
    assert boot(exe,tmp_path,sd=False)['ssid']=='first'

def test_unconfirmed_network_timeout_restores_previous_settings(exe,tmp_path):
    card(tmp_path,config(),1700000000);boot(exe,tmp_path,'confirm')
    card(tmp_path,config('wrong-network'),1700000100)
    assert boot(exe,tmp_path,'rollback')['ssid']=='first'
    assert boot(exe,tmp_path,sd=False)['ssid']=='first'

def test_no_saved_config_and_no_card_is_unconfigured(exe,tmp_path):
    assert not boot(exe,tmp_path,sd=False)['loaded']
