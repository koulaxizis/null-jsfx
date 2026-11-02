#!/usr/bin/env python3
"""Build the media kits of NULL JSFX, orOS and noxpress (and the combined kit).

Renders wallpapers, logos, banners and Printful print files with headless Chromium
(Playwright), writes palettes and SVG sources, copies fonts and texts, and packs the
zips that the media-kit pre-release serves:

    <Project>-Media-Kit.zip and <Project>-Wallpapers.zip for NULL-JSFX, orOS,
    noxpress and NULL-JSFX-orOS-noxpress (all three).

Usage:  python3 tools/media_kit/build.py OUT_DIR [--previews DIR]

Dark versions only (Christos, 2026-11-02). Needs Node with the `playwright` package and
its Chromium (`npm ci` in tests/remote is enough), and Python with Pillow and NumPy.
Takes a few minutes. The workflow media-kit.yml runs it and uploads the zips.
"""
import argparse, json, os, shutil, struct, subprocess, tempfile, zipfile
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
NOTO = os.path.join(ROOT, 'hub', 'ui', 'fonts')
TEXT = os.path.join(HERE, 'text')

SUBJ = {'null': 'null-jsfx', 'oros': 'oros', 'nox': 'noxpress', 'all': 'all-three'}
WALL_SIZES = [(3840, 2160, 'desktop-4k'), (5120, 2880, 'desktop-5k'), (2560, 1600, 'desktop-16x10'),
              (3440, 1440, 'ultrawide'), (1440, 3200, 'android'), (1290, 2796, 'iphone'),
              (2732, 2048, 'tablet-landscape'), (2048, 2732, 'tablet-portrait')]
BANNERS = [('open-graph', 1200, 630, {}), ('kofi-x-header', 1500, 500, {'sw': '0.5', 'sh': '0.45'}),
           ('youtube-banner', 2560, 1440, {'sw': '0.55', 'sh': '0.26'}),
           ('facebook-cover', 1640, 624, {'sw': '0.5', 'oy': '0.42'}),
           ('linkedin-banner', 1584, 396, {'sw': '0.45', 'sh': '0.5'}), ('github-social', 1280, 640, {})]
LOGO_VARIANTS = ['dark', 'white', 'black']
KITS = {'null-jsfx': ('NULL-JSFX', ['null-jsfx']), 'oros': ('orOS', ['oros']),
        'noxpress': ('noxpress', ['noxpress']),
        'all-three': ('NULL-JSFX-orOS-noxpress', ['null-jsfx', 'oros', 'noxpress', 'all-three'])}

PAL = {
    'null-jsfx': [('Background', '#1b1a18'), ('Background deep', '#131211'), ('Card', '#232220'), ('Border', '#333333'),
                  ('Text', '#e0e0e0'), ('Muted', '#999999'), ('Silver accent', '#c4c4c4'), ('Silver light', '#d4d4d4'),
                  ('Silver dark', '#9a9a9a'), ('Accent on light', '#555555')],
    'oros': [('Background', '#14120d'), ('Desktop', '#1a1712'), ('Panel', '#1d1a13'), ('Border', '#322d20'),
             ('Text', '#f0ead9'), ('Text dim', '#a89f8a'), ('Gold', '#d4af37'), ('Gold hover', '#ecc75f'),
             ('Light background', '#faf7ef'), ('Gold on light', '#b8860b')],
    'noxpress': [('Background', '#0d0f14'), ('Panel', '#101218'), ('Panel 2', '#1d2029'), ('Border', '#2e3240'),
                 ('Text', '#eae8fa'), ('Heading', '#f6f4ff'), ('Muted', '#b7b2d6'), ('Violet', '#6d4aff'),
                 ('Violet hover', '#8263ff'), ('Link', '#beb1ff')],
}
FONTS = {'null-jsfx': [os.path.join(NOTO, 'NotoSans-Regular.ttf'), os.path.join(NOTO, 'NotoSans-Bold.ttf'),
                       (os.path.join(NOTO, 'OFL.txt'), 'OFL.txt')],
         'oros': ['Nunito.ttf', ('Nunito-OFL.txt', 'OFL.txt'), 'MPLUSRounded1c-Greek-regular.ttf',
                  'MPLUSRounded1c-Greek-medium.ttf', 'MPLUSRounded1c-Greek-bold.ttf',
                  'MPLUSRounded1c-Greek-extrabold.ttf', 'OFL-MPLUSRounded1c.txt',
                  (os.path.join(TEXT, 'fonts-oros-README.md'), 'README.md')],
         'noxpress': ['Inter.ttf', ('Inter-OFL.txt', 'OFL.txt')]}
SV = ('<linearGradient id="g" x1="0%" y1="100%" x2="0%" y2="0%"><stop offset="0%" stop-color="#9a9a9a"/>'
      '<stop offset="100%" stop-color="#d4d4d4"/></linearGradient>')
SVGS = {
    'null-jsfx': ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 80"><defs>' + SV + '</defs><rect x="2" y="2" width="76" height="76" rx="14" fill="#232220" stroke="#4a4a4a" stroke-width="1.5"/><path d="M24 20 L24 60 L56 20 L56 60" stroke="url(#g)" stroke-width="7" stroke-linejoin="round" stroke-linecap="round" fill="none"/></svg>',
                  '<svg xmlns="http://www.w3.org/2000/svg" viewBox="20 16 40 48"><path d="M24 20 L24 60 L56 20 L56 60" stroke="{c}" stroke-width="7" stroke-linejoin="round" stroke-linecap="round" fill="none"/></svg>', '#c4c4c4'),
    'oros': ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" fill="none"><rect width="512" height="512" rx="96" fill="#1b1a18"/><path d="M256 104 L460 408 L52 408 Z M256 104 L342 246 L388 176 L460 408" stroke="#d4af37" stroke-width="20" stroke-linecap="round" stroke-linejoin="miter" stroke-miterlimit="3" fill="none"/></svg>',
             '<svg xmlns="http://www.w3.org/2000/svg" viewBox="36 84 440 340" fill="none"><path d="M256 104 L460 408 L52 408 Z M256 104 L342 246 L388 176 L460 408" stroke="{c}" stroke-width="20" stroke-linecap="round" stroke-linejoin="miter" stroke-miterlimit="3" fill="none"/></svg>', '#d4af37'),
    'noxpress': ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="14" fill="#0d0f14"/><path d="M21 45V27a11 11 0 0 1 22 0v18" fill="none" stroke="#6d4aff" stroke-width="8" stroke-linecap="round"/></svg>',
                 '<svg xmlns="http://www.w3.org/2000/svg" viewBox="15 10 34 41"><path d="M21 45V27a11 11 0 0 1 22 0v18" fill="none" stroke="{c}" stroke-width="8" stroke-linecap="round"/></svg>', '#6d4aff'),
}


def rgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def cmyk(h):
    r, g, b = [x / 255 for x in rgb(h)]
    k = 1 - max(r, g, b)
    if k >= 1:
        return (0, 0, 0, 100)
    return tuple(round(v * 100) for v in ((1 - r - k) / (1 - k), (1 - g - k) / (1 - k), (1 - b - k) / (1 - k), k))


def ase(name, cols):
    def s(t):
        t = (t + '\0').encode('utf-16-be')
        return struct.pack('>H', len(t) // 2) + t
    blocks = []
    g = s(name)
    blocks.append(struct.pack('>HI', 0xc001, len(g)) + g)
    for n, h in cols:
        body = s(n) + b'RGB ' + struct.pack('>fff', *[c / 255 for c in rgb(h)]) + struct.pack('>H', 0)
        blocks.append(struct.pack('>HI', 0x0001, len(body)) + body)
    blocks.append(struct.pack('>HI', 0xc002, 0))
    return b'ASEF' + struct.pack('>HHI', 1, 0, len(blocks)) + b''.join(blocks)


def jobs_all():
    jobs = []
    J = lambda **k: jobs.append(k)
    for s, n in SUBJ.items():
        for st in ['mark', 'pattern', 'minimal']:
            for w, h, label in WALL_SIZES:
                J(page='wall.html', q=dict(s=s, st=st, t='dark'), w=w, h=h, kind='wall',
                  out=f'raw/{n}/{n}-{st}-dark-{label}-{w}x{h}.png')
        J(page='print.html', q=dict(it='tshirt', s=s, v='dark'), w=3600, h=4800, transparent=True, kind='print',
          out=f'print/{n}/{n}-tshirt-front-dark-garment-3600x4800.png')
        J(page='print.html', q=dict(it='mug', s=s, v='dark'), w=2700, h=1050, kind='print',
          out=f'print/{n}/{n}-mug-11oz-wrap-dark-2700x1050.png')
        J(page='print.html', q=dict(it='sticker', s=s), w=1200, h=1200, transparent=True, kind='print',
          out=f'print/{n}/{n}-sticker-4in-1200x1200.png')
        d = f'logos/{n}'
        for px in [512, 1024, 2048, 4096]:
            if s == 'all':
                J(page='logo.html', q=dict(s=s, kind='icon', v='dark'), w=px, h=px, transparent=True, fitW=px,
                  out=f'{d}/{n}-icons-dark-{px}w.png')
            else:
                J(page='logo.html', q=dict(s=s, kind='icon', v='dark'), w=px, h=px, transparent=True,
                  out=f'{d}/{n}-icon-{px}.png')
        for v in LOGO_VARIANTS:
            if s != 'all':
                J(page='logo.html', q=dict(s=s, kind='mark', v=v), w=2048, h=2048, transparent=True,
                  out=f'{d}/{n}-mark-{v}-2048.png')
                J(page='logo.html', q=dict(s=s, kind='lockup-v', v=v), w=2048, h=2048, transparent=True, fitW=2400,
                  out=f'{d}/{n}-logo-stacked-{v}.png')
                J(page='logo.html', q=dict(s=s, kind='lockup-h', v=v, tag='0'), w=2048, h=512, transparent=True,
                  fitW=4000, out=f'{d}/{n}-logo-horizontal-notag-{v}.png')
            J(page='logo.html', q=dict(s=s, kind='lockup-h', v=v), w=2048, h=512, transparent=True, fitW=4000,
              out=f'{d}/{n}-logo-horizontal-{v}.png')
        for name, w, h, extra in BANNERS:
            J(page='logo.html', q=dict(s=s, kind='banner', v='dark', **extra), w=w, h=h,
              out=f'banners/{n}/{n}-{name}-dark-{w}x{h}.png')
        J(page='logo.html', q=dict(s=s, kind='avatar', v='dark'), w=1080, h=1080,
          out=f'banners/{n}/{n}-avatar-dark-1080.png')
    return jobs


def render(work, out):
    jobs = jobs_all()
    for j in jobs:
        j['out'] = os.path.join(out, j['out'])
    with open(os.path.join(work, 'jobs.json'), 'w') as f:
        json.dump(jobs, f)
    # Playwright comes from tests/remote (npm ci there), or from a global install
    env = dict(os.environ)
    env['NODE_PATH'] = os.pathsep.join(filter(None, [env.get('NODE_PATH'),
                                                    os.path.join(ROOT, 'tests', 'remote', 'node_modules')]))
    subprocess.run(['node', 'render.js', 'jobs.json'], cwd=work, check=True, env=env)
    rng = np.random.default_rng(1)
    for j in jobs:
        if j.get('kind') == 'wall':  # luminance dither against banding, then JPEG
            im = np.asarray(Image.open(j['out']).convert('RGB')).astype(np.float32)
            im += rng.uniform(-1.2, 1.2, im.shape[:2])[..., None]
            dst = j['out'].replace(os.sep + 'raw' + os.sep, os.sep + 'wallpapers' + os.sep).replace('.png', '.jpg')
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            Image.fromarray(np.clip(im + 0.5, 0, 255).astype(np.uint8)).save(dst, quality=94, subsampling=0, optimize=True)
            os.remove(j['out'])
        elif j.get('kind') == 'print':  # Printful reads the dpi
            im = Image.open(j['out'])
            im.load()
            im.save(j['out'], dpi=(300, 300), optimize=True)
    shutil.rmtree(os.path.join(out, 'raw'), ignore_errors=True)


def assets(out):
    fb = ImageFont.truetype(os.path.join(NOTO, 'NotoSans-Bold.ttf'), 28)
    fr = ImageFont.truetype(os.path.join(NOTO, 'NotoSans-Regular.ttf'), 22)
    for kit, cols in PAL.items():
        d = f'{out}/palettes/{kit}'
        os.makedirs(d, exist_ok=True)
        slug = lambda n: n.lower().replace(' ', '-')
        json.dump([{'name': n, 'hex': h, 'rgb': rgb(h), 'cmyk_approx': cmyk(h)} for n, h in cols],
                  open(f'{d}/{kit}-palette.json', 'w'), indent=1)
        open(f'{d}/{kit}-palette.css', 'w').write(':root {\n' + ''.join(f'  --{kit}-{slug(n)}: {h};\n' for n, h in cols) + '}\n')
        open(f'{d}/{kit}-palette.gpl', 'w').write(f'GIMP Palette\nName: {kit}\nColumns: 5\n#\n' +
                                                    ''.join('%3d %3d %3d\t%s\n' % (*rgb(h), n) for n, h in cols))
        open(f'{d}/{kit}-palette.ase', 'wb').write(ase(kit, cols))
        im = Image.new('RGB', (1600, 900), '#ffffff')
        dr = ImageDraw.Draw(im)
        for i, (n, h) in enumerate(cols):
            x, y = (i % 5) * 320, (i // 5) * 450
            dr.rectangle([x, y, x + 320, y + 300], fill=h)
            dr.text((x + 20, y + 315), n, font=fb, fill='#111111')
            dr.text((x + 20, y + 352), f'{h.upper()}   RGB {",".join(map(str, rgb(h)))}', font=fr, fill='#333333')
            dr.text((x + 20, y + 384), 'CMYK ≈ ' + ' '.join(map(str, cmyk(h))), font=fr, fill='#333333')
        im.save(f'{d}/{kit}-palette.png')
        fd = f'{out}/fonts/{kit}'
        os.makedirs(fd, exist_ok=True)
        for f in FONTS[kit]:
            src, name = f if isinstance(f, tuple) else (f, os.path.basename(f))
            shutil.copy(src if os.path.isabs(src) else os.path.join(HERE, 'fonts', src), os.path.join(fd, name))
    for kit, (icon, markt, col) in SVGS.items():
        d = f'{out}/logos/{kit}/svg'
        os.makedirs(d, exist_ok=True)
        open(f'{d}/{kit}-icon.svg', 'w').write(icon + '\n')
        for name, c in {'color': col, 'white': '#ffffff', 'black': '#000000'}.items():
            open(f'{d}/{kit}-mark-{name}.svg', 'w').write(markt.replace('{c}', c) + '\n')


def pack(out, dest):
    os.makedirs(dest, exist_ok=True)
    made = []
    for k, (name, parts) in KITS.items():
        kit = f'{name}-Media-Kit'
        z = zipfile.ZipFile(f'{dest}/{kit}.zip', 'w', zipfile.ZIP_DEFLATED)
        add = lambda src, arc: z.write(src, f'{kit}/{arc}')
        add(f'{TEXT}/README-{k}.md', 'README.md')
        for f in ['LICENSE-media-kit.md', 'brand-guidelines.en.md', 'brand-guidelines.el.md']:
            add(f'{TEXT}/{f}', f)
        add(f'{TEXT}/print-README.md', 'print/README.md')
        for p in parts:
            for sec in ['wallpapers', 'logos', 'banners', 'print', 'palettes', 'fonts']:
                d = f'{out}/{sec}/{p}'
                if not os.path.isdir(d):
                    continue
                for root, _, fs in sorted(os.walk(d)):
                    for f in sorted(fs):
                        src = os.path.join(root, f)
                        rel = os.path.relpath(src, f'{out}/{sec}') if len(parts) > 1 else os.path.relpath(src, d)
                        add(src, f'{sec}/{rel}')
            for lang in ['en', 'el']:
                add(f'{TEXT}/press/{p}-press-release.{lang}.md', f'press/{p}-press-release.{lang}.md')
        z.close()
        wn = f'{name}-Wallpapers'
        z = zipfile.ZipFile(f'{dest}/{wn}.zip', 'w', zipfile.ZIP_STORED)
        for root, _, fs in os.walk(f'{out}/wallpapers/{k}'):
            for f in sorted(fs):
                z.write(os.path.join(root, f), f'{wn}/{f}')
        z.close()
        made += [f'{kit}.zip', f'{wn}.zip']
    return made


def previews(out, dest):
    """Small JPEG previews for the site's media-kit pages."""
    os.makedirs(dest, exist_ok=True)
    for k in SUBJ.values():
        im = Image.open(f'{out}/wallpapers/{k}/{k}-mark-dark-desktop-4k-3840x2160.jpg')
        im.resize((960, 540), Image.LANCZOS).save(f'{dest}/{k}-wallpaper.jpg', quality=82, optimize=True)
        ims = [Image.open(f'{out}/wallpapers/{k}/{k}-{st}-dark-android-1440x3200.jpg').resize((216, 480), Image.LANCZOS)
               for st in ['mark', 'pattern', 'minimal']]
        s = Image.new('RGB', (3 * 216 + 2 * 16, 480), '#111214')
        for i, p in enumerate(ims):
            s.paste(p, (i * 232, 0))
        s.save(f'{dest}/{k}-phone.jpg', quality=82, optimize=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--previews')
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    files = os.path.join(out, 'files')
    with tempfile.TemporaryDirectory() as work:
        for f in ['wall.html', 'logo.html', 'print.html', 'brand.js', 'render.js']:
            shutil.copy(os.path.join(HERE, f), work)
        for f in os.listdir(os.path.join(HERE, 'fonts')):
            shutil.copy(os.path.join(HERE, 'fonts', f), work)
        for f in ['NotoSans-Regular.ttf', 'NotoSans-Bold.ttf']:
            shutil.copy(os.path.join(NOTO, f), work)
        render(work, files)
    assets(files)
    made = pack(files, os.path.join(out, 'zips'))
    if a.previews:
        previews(files, a.previews)
    for m in made:
        print(m, os.path.getsize(os.path.join(out, 'zips', m)))


if __name__ == '__main__':
    main()
