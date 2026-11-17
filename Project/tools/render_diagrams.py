"""Render every Mermaid diagram in docs/design/diagrams/**/*.md to a PNG in
docs/design/diagrams/images/ (same relative path), using headless Chrome.

Usage (from any directory):
    python3 -I Project/tools/render_diagrams.py
"""
import glob
import json
import os
import re
import subprocess
import tempfile
import urllib.request

TOOLS = os.path.dirname(os.path.abspath(__file__))
DIAGRAMS = os.path.normpath(os.path.join(TOOLS, '..', 'docs', 'design', 'diagrams'))
OUT = os.path.join(DIAGRAMS, 'images')
MERMAID_VERSION = '11.4.1'
MERMAID_JS = os.path.join(TOOLS, '.cache', f'mermaid-{MERMAID_VERSION}.min.js')
MERMAID_URL = f'https://cdn.jsdelivr.net/npm/mermaid@{MERMAID_VERSION}/dist/mermaid.min.js'
CHROME = ['google-chrome', '--headless=new', '--disable-gpu', '--hide-scrollbars']

TPL = """<!doctype html><html><head><meta charset="utf-8"><script>%s</script></head>
<body style="margin:0;background:#fff"><div id="out"></div><script>
mermaid.initialize({startOnLoad:false, theme:'default', htmlLabels:false,
  flowchart:{htmlLabels:false}, fontFamily:'DejaVu Sans, Arial, sans-serif'});
const done = () => document.querySelectorAll('script').forEach(s => s.remove());
mermaid.render('d', %s).then(r => { document.getElementById('out').innerHTML = r.svg; done(); })
  .catch(e => { document.getElementById('out').textContent = 'MERMAID' + '_ERROR ' + e.message; done(); });
</script></body></html>"""


def load_mermaid():
    if not os.path.exists(MERMAID_JS):
        os.makedirs(os.path.dirname(MERMAID_JS), exist_ok=True)
        print(f'Downloading mermaid {MERMAID_VERSION} ...')
        urllib.request.urlretrieve(MERMAID_URL, MERMAID_JS)
    return open(MERMAID_JS, encoding='utf-8').read()


def main():
    mermaid = load_mermaid()
    sources = sorted(f for f in glob.glob(os.path.join(DIAGRAMS, '**', '*.md'), recursive=True)
                     if not f.startswith(OUT + os.sep))
    fails = []
    with tempfile.TemporaryDirectory() as tmp:
        page, shot = os.path.join(tmp, 'page.html'), os.path.join(tmp, 'shot.html')
        for src in sources:
            rel = os.path.relpath(src, DIAGRAMS)[:-3]
            block = re.search(r'```mermaid\n(.*?)```', open(src, encoding='utf-8').read(), re.S)
            if not block:
                continue
            with open(page, 'w', encoding='utf-8') as f:
                f.write(TPL % (mermaid, json.dumps(block.group(1))))
            dom = subprocess.run(CHROME + ['--virtual-time-budget=15000', '--dump-dom', 'file://' + page],
                                 capture_output=True, text=True, timeout=120).stdout
            m = re.search(r'<div id="out">(<svg.*</svg>)', dom, re.S)
            if 'MERMAID_ERROR' in dom or not m:
                err = re.search(r'MERMAID_ERROR[^<]*', dom)
                fails.append((rel, err.group(0).strip() if err else 'no svg produced'))
                continue
            svg = m.group(1)
            vb = [float(x) for x in re.search(r'viewBox="([^"]+)"', svg).group(1).split()]
            w, h = int(vb[2]) + 2, int(vb[3]) + 2
            svg = re.sub(r'style="max-width:[^"]*"', '', svg, count=1)
            svg = re.sub(r'width="100%"', f'width="{w}" height="{h}"', svg, count=1)
            with open(shot, 'w', encoding='utf-8') as f:
                f.write('<!doctype html><html><head><meta charset="utf-8"></head>'
                        f'<body style="margin:0;background:#fff">{svg}</body></html>')
            png = os.path.join(OUT, rel + '.png')
            os.makedirs(os.path.dirname(png), exist_ok=True)
            if os.path.exists(png):
                os.remove(png)
            subprocess.run(CHROME + ['--force-device-scale-factor=2', f'--window-size={w},{h}',
                                     '--default-background-color=FFFFFFFF', f'--screenshot={png}',
                                     'file://' + shot], capture_output=True, timeout=120)
            print(f'{rel}.png  {w}x{h}', 'OK' if os.path.exists(png) else 'MISSING')
    print('FAILED:', fails if fails else 'none')


if __name__ == '__main__':
    main()
