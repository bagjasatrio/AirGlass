import json
import os
import subprocess
import time
import urllib.parse
import urllib.request

def test_web_drop():
    p = subprocess.Popen(['build/Release/airglass.exe'])
    try:
        time.sleep(2)
        # 1. Status
        with urllib.request.urlopen('http://127.0.0.1:53317/api/status', timeout=3) as resp:
            data = json.loads(resp.read().decode())
            assert data.get('app') == 'AirGlass'
            assert data.get('web_drop') is True

        # 2. Upload
        boundary = '----WebKitFormBoundary7MA4YWxkTrZu0gW'
        body = (
            f'--{boundary}\r\n'
            'Content-Disposition: form-data; name="file"; filename="airglass_test.txt"\r\n'
            'Content-Type: text/plain\r\n\r\n'
            'AirGlass test transfer content'
            f'\r\n--{boundary}--\r\n'
        ).encode('utf-8')

        req = urllib.request.Request(
            'http://127.0.0.1:53317/api/upload',
            data=body,
            headers={'Content-Type': f'multipart/form-data; boundary={boundary}'}
        )
        with urllib.request.urlopen(req, timeout=3) as resp:
            res = json.loads(resp.read().decode())
            assert res.get('success') is True
            assert res.get('saved') == 1

        # 3. List: Verify uploaded iOS file does NOT leak into outgoing shared files
        with urllib.request.urlopen('http://127.0.0.1:53317/api/files', timeout=3) as resp:
            files = json.loads(resp.read().decode())
            assert not any(f['name'] == 'airglass_test.txt' for f in files)

        # 4. Outgoing file test: Place a file in AppData/Local/AirGlass/SharedToIOS staging folder
        local_app_data = os.environ.get('LOCALAPPDATA', os.path.expanduser('~'))
        shared_dir = os.path.join(local_app_data, 'AirGlass', 'SharedToIOS')
        os.makedirs(shared_dir, exist_ok=True)
        shared_file = os.path.join(shared_dir, 'outgoing_to_ios.txt')
        with open(shared_file, 'w') as sf:
            sf.write('Hello from PC to iPhone')

        with urllib.request.urlopen('http://127.0.0.1:53317/api/files', timeout=3) as resp:
            files = json.loads(resp.read().decode())
            assert any(f['name'] == 'outgoing_to_ios.txt' for f in files)

        # 5. Download outgoing file
        dl_url = 'http://127.0.0.1:53317/api/download?name=' + urllib.parse.quote('outgoing_to_ios.txt')
        with urllib.request.urlopen(dl_url, timeout=3) as resp:
            content = resp.read().decode()
            assert content == 'Hello from PC to iPhone'

        print('PASS: test_web_drop all separation checks passed')
    finally:
        p.terminate()
        p.wait()
        # Clean up
        downloads = os.path.expanduser('~/Downloads/AirGlass')
        fpath = os.path.join(downloads, 'airglass_test.txt')
        if os.path.exists(fpath):
            os.remove(fpath)

if __name__ == '__main__':
    test_web_drop()
