#!/usr/bin/env python3
# Makes the pictures of the icons in ../amigaApps: each icon's 30x30 image in the 8
# colours of MagicWB (normal and selected), written over the one it has, and after the
# icon a 46x46 GlowIcon (FORM ICON, which icon.library 44 - OS 3.5 on - shows instead).
# The rest of an icon - its type, default tool, tool types - stays as it is.
#
# The pictures are drawn in unit coordinates (0..1), so that one design gives both
# sizes; the mark in the corner tells where the tool shows things: the robot of
# Android for a window of Android, the Amiga's check mark (as Aminet marks software
# for 68k) for a window of the Amiga.
#
# Usage: python3 makeicons.py [--preview sheet.png] [folder]
# folder defaults to ../amigaApps; --preview draws the pictures, enlarged, into a
# PNG instead of writing the icons. Needs Pillow.

import sys, struct, os
from PIL import Image, ImageDraw, ImageChops

MWB = [(0x95,0x95,0x95),(0,0,0),(255,255,255),(0x3b,0x67,0xa2),
       (0x7b,0x7b,0x7b),(0xaf,0xaf,0xaf),(0xaa,0x90,0x7c),(0xff,0xa9,0x97)]
# colour names: true colour for GlowIcons, MagicWB pen for the planar image
COL = {
 'black':  ((0,0,0),1), 'white':((255,255,255),2), 'blue':((0x3b,0x67,0xa2),3),
 'sky':    ((0x6e,0xa8,0xe8),3), 'dark':((0x60,0x60,0x60),4), 'grey':((0x95,0x95,0x95),0),
 'light':  ((0xc8,0xc8,0xc8),5), 'beige':((0xaa,0x90,0x7c),6), 'pink':((0xff,0xa9,0x97),7),
 'red':    ((0xe0,0x20,0x20),7), 'green':((0x3d,0xdc,0x84),2), 'yellow':((0xff,0xd0,0x40),7),
 'navy':   ((0x1e,0x3c,0x78),3),
 'tred':   ((0xed,0x3c,0x34),7), 'torange':((0xee,0x64,0x2c),7), 'tamber':((0xef,0xa8,0x2c),6),
 'tyellow':((0xf3,0xe7,0x2c),2), 'tlime':((0xbd,0xe6,0x2d),5), 'tgreen':((0x51,0xf6,0x3b),5),
 'tblue':  ((0x2c,0x93,0xde),3), 'film':((0x30,0x30,0x30),1), 'brown':((0x8a,0x60,0x40),6),
}

class Canvas:
    def __init__(self, size, glow):
        self.n = size; self.glow = glow
        self.s = 8 if glow else 1          # supersampling for the GlowIcon
        if glow:
            self.im = Image.new('RGBA', (size*8, size*8), (0,0,0,0))
        else:
            self.im = Image.new('P', (size, size), 0)
            pal = []
            for c in MWB: pal += list(c)
            self.im.putpalette(pal + [0]*(768-len(pal)))
        self.d = ImageDraw.Draw(self.im)
    def c(self, name):
        rgb, pen = COL[name]
        return rgb + (255,) if self.glow else pen
    def p(self, x, y):
        k = (self.n - (0 if self.glow else 1)) * self.s
        return (x * k, y * k)
    def box(self, x0, y0, x1, y1):
        a = self.p(x0, y0); b = self.p(x1, y1)
        return [a[0], a[1], b[0], b[1]]
    def w(self, u):
        return max(1, round(u * self.n * self.s)) if self.glow else max(1, round(u * self.n))
    def rect(self, x0,y0,x1,y1, fill=None, outline=None, width=0.04, r=0):
        f = self.c(fill) if fill else None; o = self.c(outline) if outline else None
        if r and self.glow:
            self.d.rounded_rectangle(self.box(x0,y0,x1,y1), radius=r*self.n*self.s, fill=f, outline=o, width=self.w(width) if o else 0)
        else:
            self.d.rectangle(self.box(x0,y0,x1,y1), fill=f, outline=o, width=self.w(width) if o else 0)
    def ellipse(self, x0,y0,x1,y1, fill=None, outline=None, width=0.04):
        self.d.ellipse(self.box(x0,y0,x1,y1), fill=self.c(fill) if fill else None,
            outline=self.c(outline) if outline else None, width=self.w(width) if outline else 0)
    def pieslice(self, x0,y0,x1,y1, a0,a1, fill):
        self.d.pieslice(self.box(x0,y0,x1,y1), a0, a1, fill=self.c(fill))
    def arc(self, x0,y0,x1,y1, a0,a1, col, width=0.04):
        self.d.arc(self.box(x0,y0,x1,y1), a0, a1, fill=self.c(col), width=self.w(width))
    def poly(self, pts, fill=None, outline=None, width=0.035):
        q = [self.p(x,y) for x,y in pts]
        self.d.polygon(q, fill=self.c(fill) if fill else None,
            outline=self.c(outline) if outline and not self.glow else None)
        if outline and self.glow:
            self.d.line(q + [q[0]], fill=self.c(outline), width=self.w(width), joint='curve')
    def line(self, pts, col, width=0.04):
        self.d.line([self.p(x,y) for x,y in pts], fill=self.c(col), width=self.w(width))
    def done(self):
        if self.glow:
            return self.im.resize((self.n, self.n), Image.LANCZOS)
        return self.im

# --- the pictures ----------------------------------------------------------
def shadow_window(cv, x0,y0,x1,y1, title='blue'):
    cv.rect(x0+0.04,y0+0.04,x1+0.04,y1+0.04, fill='dark')
    cv.rect(x0,y0,x1,y1, fill='white', outline='black', width=0.035)
    cv.rect(x0,y0,x1,y0+0.13, fill=title, outline='black', width=0.035)

def globe(cv, cx, cy, r):
    cv.ellipse(cx-r,cy-r,cx+r,cy+r, fill='sky', outline='navy', width=0.04)
    cv.ellipse(cx-r*0.45,cy-r,cx+r*0.45,cy+r, outline='white', width=0.03)
    cv.line([(cx-r,cy),(cx+r,cy)], 'white', 0.03)
    if not cv.glow:     # at 30 pixels more lines are a muddle
        return
    cv.line([(cx,cy-r),(cx,cy+r)], 'white', 0.03)
    cv.arc(cx-r*0.95,cy-r*1.35,cx+r*0.95,cy-r*0.25, 20,160, 'white', 0.03)
    cv.arc(cx-r*0.95,cy+r*0.25,cx+r*0.95,cy+r*1.35, 200,340, 'white', 0.03)

def browser(cv):
    shadow_window(cv, 0.05,0.08,0.86,0.84)
    globe(cv, 0.45, 0.50, 0.26)

def browser_new(cv):
    shadow_window(cv, 0.02,0.02,0.62,0.55, title='dark')
    shadow_window(cv, 0.24,0.26,0.94,0.92)
    globe(cv, 0.59, 0.62, 0.22)

def video(cv):
    cv.rect(0.10,0.06,0.80,0.90, fill='film', outline='black', width=0.03)
    for i in range(6):
        y = 0.10 + i*0.135
        cv.rect(0.14,y,0.21,y+0.07, fill='light')
        cv.rect(0.69,y,0.76,y+0.07, fill='light')
    cv.rect(0.26,0.14,0.64,0.82, fill='sky')
    cv.poly([(0.36,0.30),(0.36,0.66),(0.58,0.48)], fill='white', outline='black')

def picture(cv):
    cv.rect(0.08,0.12,0.86,0.82, fill='beige', outline='black', width=0.035)
    cv.rect(0.16,0.20,0.78,0.74, fill='sky')
    cv.ellipse(0.56,0.25,0.70,0.39, fill='yellow')
    cv.poly([(0.16,0.74),(0.36,0.42),(0.52,0.62),(0.62,0.50),(0.78,0.74)], fill='dark')
    cv.poly([(0.30,0.52),(0.36,0.42),(0.42,0.51)], fill='white')

def search(cv):
    cv.line([(0.58,0.58),(0.86,0.86)], 'black', 0.13)
    cv.line([(0.60,0.60),(0.84,0.84)], 'brown', 0.07)
    cv.ellipse(0.08,0.08,0.66,0.66, fill='white', outline='blue', width=0.08)
    cv.arc(0.18,0.18,0.56,0.56, 200,260, 'sky', 0.05)

def floppy(cv):
    cv.poly([(0.10,0.08),(0.78,0.08),(0.88,0.18),(0.88,0.90),(0.10,0.90)], fill='blue', outline='black')
    cv.rect(0.28,0.08,0.68,0.36, fill='light', outline='black', width=0.03)
    cv.rect(0.52,0.12,0.62,0.32, fill='dark')
    cv.rect(0.20,0.50,0.78,0.90, fill='white', outline='black', width=0.03)
    for i in range(3):
        y = 0.60 + i*0.09
        cv.line([(0.27,y),(0.71,y)], 'dark', 0.025)

def tube(cv):
    cv.rect(0.06,0.20,0.88,0.78, fill='red', outline='black', width=0.035, r=0.14)
    cv.poly([(0.38,0.33),(0.38,0.65),(0.64,0.49)], fill='white')

def forum(cv):
    cv.rect(0.04,0.08,0.66,0.50, fill='white', outline='black', width=0.035, r=0.08)
    cv.poly([(0.16,0.50),(0.16,0.64),(0.30,0.50)], fill='white', outline='black')
    cv.rect(0.30,0.36,0.92,0.76, fill='pink', outline='black', width=0.035, r=0.08)
    cv.poly([(0.78,0.76),(0.78,0.90),(0.64,0.76)], fill='pink', outline='black')
    for i in range(2):
        cv.line([(0.12,0.20+i*0.12),(0.56,0.20+i*0.12)], 'dark', 0.03)
        cv.line([(0.38,0.48+i*0.12),(0.84,0.48+i*0.12)], 'dark', 0.03)

# --- the marks: Android or the Amiga's own window ---------------------------
def tick(cv, x0, y0, s):
    # the Amiga's check mark, as Aminet marks software for 68k: in bands of colour, top down,
    # its short arm blue
    U = lambda x, y: cv.p(x0 + x*s, y0 + y*s)
    if cv.glow:
        shape = [(0.68,0.0),(1.0,0.0),(0.48,1.0),(0.30,1.0),(0.0,0.60),(0.24,0.60),(0.38,0.80)]
    else:       # bolder, at a dozen pixels
        shape = [(0.55,0.0),(1.0,0.0),(0.52,1.0),(0.20,1.0),(0.0,0.55),(0.30,0.55),(0.36,0.66)]
    mask = Image.new('L', cv.im.size, 0)
    ImageDraw.Draw(mask).polygon([U(x,y) for x,y in shape], fill=255)
    bands = [(0.0,0.18,'tred'),(0.18,0.33,'torange'),(0.33,0.46,'tamber'),(0.46,0.62,'tyellow'),
             (0.62,0.78,'tlime'),(0.78,1.01,'tgreen')]
    for ya, yb, col in bands + [(0, 0, None)]:
        band = Image.new('L', cv.im.size, 0)
        if col is None:     # the short arm
            col = 'tblue'
            ImageDraw.Draw(band).rectangle([U(-0.1,0.5), U(0.31,1.1)], fill=255)
        else:
            ImageDraw.Draw(band).rectangle([U(-0.1,ya), U(1.1,yb)], fill=255)
        cv.im.paste(cv.c(col), mask=ImageChops.multiply(band, mask))
    q = [U(x,y) for x,y in shape]
    if cv.glow:
        cv.d.line(q + [q[0]], fill=cv.c('black'), width=cv.w(0.022), joint='curve')
    else:
        cv.d.polygon(q, outline=cv.c('black'))

def android(cv, x0, y0, s):
    # the head of the robot: a half disc with two antennas and eyes
    cv.line([(x0+s*0.28,y0+s*0.30),(x0+s*0.14,y0+s*0.08)], 'black', 0.03)
    cv.line([(x0+s*0.72,y0+s*0.30),(x0+s*0.86,y0+s*0.08)], 'black', 0.03)
    cv.pieslice(x0,y0+s*0.20,x0+s,y0+s*1.20, 180,360, 'green')
    cv.arc(x0,y0+s*0.20,x0+s,y0+s*1.20, 180,360, 'black', 0.03)
    cv.line([(x0,y0+s*0.70),(x0+s,y0+s*0.70)], 'black', 0.03)
    cv.ellipse(x0+s*0.26,y0+s*0.42,x0+s*0.38,y0+s*0.54, fill='black')
    cv.ellipse(x0+s*0.62,y0+s*0.42,x0+s*0.74,y0+s*0.54, fill='black')

PICTURES = {'browser':browser, 'browser_new':browser_new, 'video':video, 'picture':picture,
            'search':search, 'floppy':floppy, 'tube':tube, 'forum':forum}

# the selected look: darker, as pressed
DARKER = {2:5, 5:4, 7:6, 6:4, 4:1, 3:3, 1:1, 0:0}
def selected(im, glow):
    if glow:
        px = im.load()
        out = im.copy(); po = out.load()
        for y in range(im.size[1]):
            for x in range(im.size[0]):
                r,g,b,a = px[x,y]
                po[x,y] = (int(r*0.7), int(g*0.7), int(b*0.7), a)
        return out
    out = im.copy()
    out.putdata([DARKER[v] for v in im.getdata()])
    return out

def render(picture, mark, size, glow):
    cv = Canvas(size, glow)
    PICTURES[picture](cv)
    if mark == 'host':
        if glow:
            tick(cv, 0.56, 0.56, 0.42)
        else:
            tick(cv, 0.52, 0.52, 0.48)
    elif mark == 'android':
        android(cv, 0.58, 0.60, 0.40)
    return cv.done()

# --- reading and writing the icons ----------------------------------------

def images(d):
    dd=struct.unpack('>I',d[66:70])[0]
    flags=struct.unpack('>H',d[16:18])[0]
    off=78+(56 if dd else 0)
    out=[]
    n=2 if flags&2 else 1
    for k in range(n):
        iw,ih,depth=struct.unpack('>HHH',d[off+4:off+10])
        pick,onoff=d[off+14],d[off+15]
        words=(iw+15)//16
        planes=bin(pick).count('1')
        data=d[off+20:off+20+words*2*ih*planes]
        px=[[0]*iw for _ in range(ih)]
        p=0
        for b in range(depth):
            if not (pick>>b)&1:
                if (onoff>>b)&1:
                    for y in range(ih):
                        for x in range(iw): px[y][x]|=1<<b
                continue
            for y in range(ih):
                row=data[(p*ih+y)*words*2:(p*ih+y+1)*words*2]
                for x in range(iw):
                    if row[x>>3]>>(7-(x&7))&1: px[y][x]|=1<<b
            p+=1
        out.append((off,iw,ih,depth,pick,onoff,px))
        off+=20+words*2*ih*planes
    return out,off

ICONS = {'AndroidBrowser':('browser','android'),'HostBrowser':('browser','host'),
 'AndroidBrowserInNewWindow':('browser_new','android'),
 'AndroidVideoPlay':('video','android'),'HostVideoPlay':('video','host'),
 'AndroidImageView':('picture','android'),'HostImageView':('picture','host'),
 'AndroidGoogle':('search','android'),'HostGoogle':('search','host'),
 'AndroidAminet':('floppy','android'),'HostAminet':('floppy','host'),
 'AndroidYouTube':('tube','android'),'HostYouTube':('tube','host'),
 'AndroidAmigaworld':('forum','android'),'HostAmigaworld':('forum','host')}
G = 46

def planes(im, depth=3):
    w, h = im.size; words = (w + 15)//16
    out = bytearray()
    px = im.load()
    for b in range(depth):
        for y in range(h):
            row = bytearray(words*2)
            for x in range(w):
                if (px[x, y] >> b) & 1:
                    row[x >> 3] |= 0x80 >> (x & 7)
            out += row
    return bytes(out)

def packbits(data):
    out = bytearray(); i = 0; n = len(data)
    while i < n:
        j = i
        while j + 1 < n and data[j+1] == data[i] and j - i < 127:
            j += 1
        if j > i:
            out += bytes([(257 - (j - i + 1)) & 0xff, data[i]]); i = j + 1
            continue
        j = i
        while j < n and j - i < 128 and not (j + 1 < n and data[j+1] == data[j]):
            j += 1
        if j == i:
            j = i + 1
        out += bytes([j - i - 1]) + data[i:j]; i = j
    return bytes(out)

def chunk(name, data):
    c = name.encode() + struct.pack('>I', len(data)) + data
    return c + (b'\0' if len(data) & 1 else b'')

def glowform(normal, sel):
    # one palette for both, index 0 transparent
    both = Image.new('RGBA', (G*2, G)); both.paste(normal, (0, 0)); both.paste(sel, (G, 0))
    rgb = Image.new('RGB', both.size, (0, 0, 0)); rgb.paste(both, mask=both.split()[3])
    q = rgb.quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:255*3]
    alpha = both.split()[3].load(); qp = q.load()
    def indices(x0):
        out = bytearray()
        for y in range(G):
            for x in range(G):
                out.append(0 if alpha[x0+x, y] < 128 else qp[x0+x, y] + 1)
        return bytes(out)
    ncol = 1 + max(max(indices(0)), max(indices(G)))
    palette = bytes([0, 0, 0]) + bytes(pal[:(ncol-1)*3])
    def imag(data, with_pal):
        rle = packbits(data)
        head = struct.pack('>BBBBBBHH', 0, ncol - 1, 1 | (2 if with_pal else 0), 1, 0, 8,
                           len(rle) - 1, (len(palette) - 1) if with_pal else 0)
        return chunk('IMAG', head + rle + (palette if with_pal else b''))
    face = struct.pack('>BBBBH', G - 1, G - 1, 0, 0x11, len(palette) - 1)
    body = b'ICON' + chunk('FACE', face) + imag(indices(0), True) + imag(indices(G), False)
    return b'FORM' + struct.pack('>I', len(body)) + body

def write(path, pic, mark):
    d = bytearray(open(path, 'rb').read())
    form = d.find(b'FORM')
    if form >= 0 and d[form+8:form+12] == b'ICON':
        del d[form:]
    imgs, end = images(bytes(d))
    pn = render(pic, mark, 30, False); ps = selected(pn, False)
    for (off, iw, ih, depth, pick, onoff, px), im in zip(imgs, (pn, ps)):
        assert (iw, ih, depth, pick) == (30, 30, 3, 7), path
        data = planes(im)
        d[off+20:off+20+len(data)] = data
    gn = render(pic, mark, G, True)
    d += glowform(gn, selected(gn, True))
    open(path, 'wb').write(d)


def preview(path):
    Z = 4; cols = 5; cw = 46*Z*2 + 30; ch = 46*Z + 30 + 30*Z
    names = list(ICONS)
    sheet = Image.new('RGB', (cols*cw, ((len(names)+cols-1)//cols)*ch), MWB[0])
    d = ImageDraw.Draw(sheet)
    for k, name in enumerate(names):
        pic, mark = ICONS[name]
        x = (k % cols)*cw + 10; y = (k // cols)*ch + 5
        g = render(pic, mark, G, True)
        for j, im in enumerate((g, selected(g, True))):
            bg = Image.new('RGBA', im.size, MWB[0] + (255,)); bg.alpha_composite(im)
            sheet.paste(bg.convert('RGB').resize((G*Z, G*Z), Image.NEAREST), (x + j*(G*Z+6), y))
        p = render(pic, mark, 30, False)
        for j, im in enumerate((p, selected(p, False))):
            sheet.paste(im.convert('RGB').resize((30*Z, 30*Z), Image.NEAREST), (x + j*(30*Z+6), y + G*Z + 4))
        d.text((x + 60*Z + 16, y + G*Z + 10), name, fill=(0, 0, 0))
    sheet.save(path)

if __name__ == '__main__':
    args = sys.argv[1:]
    if args[:1] == ['--preview']:
        preview(args[1])
        sys.exit(0)
    folder = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'amigaApps')
    for name, (pic, mark) in ICONS.items():
        write(os.path.join(folder, name + '.info'), pic, mark)
        print(name)
