r"""Makes the synthetic training lines for rec_en.onnx (task L): pixel game fonts mixed with normal fonts.

    python -I gen_lines.py --work <dir> [--train 400000] [--val 2000] [--seed 7] [--workers N]

Reads <work>\sources\manifest.json (written by fetch_sources.py) and writes PaddleOCR SimpleDataSet files:
    <work>\data\train\*.png, <work>\data\val_pixel\*.png, <work>\data\val_normal\*.png
    <work>\data\train_list.txt, val_pixel.txt, val_normal.txt, val_list.txt   ("relative/path.png<TAB>label")

What it keeps true:
- Every image is already what Lexia gives the recogniser: the crop resized to height 48 with INTER_CUBIC when it
  grows and INTER_AREA when it shrinks, the width rounded up to a multiple of 4 (src/ocr.cpp, Recognize). The model
  then learns the pixels Lexia really feeds it. Lines wider than MAX_W after that resize are made shorter.
- Every label holds only characters of dict_en.txt (plus the space), at most MAX_LEN of them, no leading, trailing
  or double spaces.
- The same --seed gives the same lines: sample i draws from its own random.Random(seed * 1_000_003 + i), so the
  number of workers does not change the data.
- Pixel fonts are drawn at their own pixel size without anti-aliasing and scaled up with nearest neighbour, as a
  game does on a big screen; normal fonts are drawn anti-aliased at screen sizes.
- The validation sets use other seeds than the training set and never contain the Fixture texts, so the check in
  check_onnx.py measures reading, not memory.
"""

import argparse
import json
import math
import multiprocessing as mp
import os
import random
import re
import sys
from functools import lru_cache

import cv2
import numpy as np
from fontTools.pens.recordingPen import RecordingPen
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
DICT_FILE = os.path.join(HERE, "dict_en.txt")
H = 48          # Lexia's recogniser input height (src/ocr.cpp)
MAX_W = 480     # widest training image; the config's image_shape uses the same width
MAX_LEN = 40    # the config's max_text_length
PIXEL_SHARE = 0.4
# Caps-only fonts whose small capitals differ too much from the capitals for caps_only() to see it.
CAPS_FONTS = ("Cinzel",)

# ------------------------------------------------------------------------------------------------- characters

with open(DICT_FILE, encoding="utf-8") as _f:
    CHARS = set(line.rstrip("\r\n") for line in _f if line.rstrip("\r\n"))
CHARS.add(" ")

# Typographic characters in the books mapped to the plain ones of the dictionary.
REPLACE = {"‘": "'", "’": "'", "“": '"', "”": '"', "—": "-", "–": "-", "…": "...",
           " ": " ", "_": " "}


def clean(text: str) -> str:
    """Maps typographic characters, drops anything outside the dictionary and squeezes spaces."""
    for a, b in REPLACE.items():
        text = text.replace(a, b)
    text = "".join(c if c in CHARS else " " for c in text)
    return re.sub(r" {2,}", " ", text).strip()


# ------------------------------------------------------------------------------------------------- texts

def load_books(paths):
    """The books' words, Project Gutenberg's header and licence footer cut off."""
    words = []
    for p in paths:
        with open(p, encoding="utf-8", errors="replace") as f:
            t = f.read()
        start = t.find("*** START")
        end = t.find("*** END")
        if start >= 0:
            t = t[t.find("\n", start) + 1:]
            end = t.find("*** END")
        if end > 0:
            t = t[:end]
        words.extend(clean(t.replace("\n", " ")).split(" "))
    return [w for w in words if w]


KEYS = ["Z", "X", "C", "A", "S", "D", "Q", "E", "R", "F", "ENTER", "Enter", "SHIFT", "Shift", "CTRL", "Ctrl", "ESC",
        "Esc", "TAB", "Tab", "SPACE", "Space", "ALT", "Alt", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9",
        "F10", "F11", "F12", "UP", "DOWN", "LEFT", "RIGHT", "Backspace", "Del", "Home", "End", "PgUp", "PgDn",
        "1", "2", "3", "4", "W", "I", "J", "K", "L", "M", "N", "O", "P", "LB", "RB", "LT", "RT"]
MENU = ["FIGHT", "ACT", "ITEM", "MERCY", "SPARE", "FLEE", "MAGIC", "DEFEND", "Save", "Load", "Continue", "Reset",
        "Options", "Settings", "Quit", "Exit", "Back", "Confirm", "Cancel", "Menu", "Inventory", "Equip", "Skills",
        "Map", "Journal", "Status", "Party", "Shop", "Buy", "Sell", "Talk", "Check", "Use", "Drop", "Info", "Stats",
        "New Game", "Begin Game", "Start", "Credits", "Controls", "Audio", "Video", "Language", "Fullscreen",
        "Windowed", "Volume", "Music", "Sound", "Border", "Difficulty", "Easy", "Normal", "Hard", "Yes", "No",
        "OK", "Retry", "Give up", "Pause", "Resume", "Level", "Score", "Time", "Lives", "Coins", "Gold", "Keys"]
LETTER_TRAPS = ["l1I|", "Il1", "O0o", "0O", "S5", "Z2", "B8", "G6", "rn", "m", "vv", "w", "cl", "d", "ll", "Il",
                "1l", "il", "Ill", "lll", "o0", "Q0", "U", "u", "V", "v", "Y", "y", "C", "c", "K", "k"]


def word(r: random.Random, books) -> str:
    return books[r.randrange(len(books))]


def book_line(r, books, max_len):
    """A run of 1-7 book words, cut to max_len."""
    i = r.randrange(len(books) - 8)
    n = r.randint(1, 7)
    s = " ".join(books[i:i + n])
    while len(s) > max_len and " " in s:
        s = s.rsplit(" ", 1)[0]
    return s[:max_len]


def game_line(r, books):
    """Game-interface text: key prompts, menus, stats, versions, dialogue with stars."""
    k = lambda: r.choice(KEYS)
    w = lambda: r.choice(MENU)
    t = r.randrange(14)
    if t == 0:
        return f"[{k()} or {k()}] - {w()}"
    if t == 1:
        return f"[{k()}] - {w()}"
    if t == 2:
        return f"[Hold {k()}] - {w()}"
    if t == 3:
        return f"Press {k()} to {word(r, books).lower()}"
    if t == 4:
        return f"{w()} ({word(r, books)})"
    if t == 5:
        hp = r.randint(1, 999)
        return f"LV {r.randint(1, 20)}  HP {r.randint(0, hp)} / {hp}"
    if t == 6:
        return f"{r.choice(['HP', 'MP', 'TP', 'SP', 'EXP', 'G', 'AT', 'DF'])} {r.randint(0, 9999)}"
    if t == 7:
        return f"v{r.randint(0, 3)}.{r.randint(0, 99):02d}"
    if t == 8:
        return f"* {book_line(r, books, 30)}"
    if t == 9:
        return f"When {r.choice(['HP', 'MP', 'TP'])} is {r.randint(0, 9)}, you {word(r, books).lower()}."
    if t == 10:
        return " ".join(w() for _ in range(r.randint(1, 4)))
    if t == 11:
        return f"{w()}: {r.choice(['ON', 'OFF', 'On', 'Off', str(r.randint(0, 100)) + '%'])}"
    if t == 12:
        return f"File {r.randint(1, 3)}  {r.randint(0, 99):02d}:{r.randint(0, 59):02d}  {word(r, books)}"
    return f"{word(r, books)} {r.choice(['got', 'used', 'found'])} the {word(r, books)}."


def trap_line(r):
    """Strings full of look-alike letters and digits mixed with words, so they are learnt from context."""
    parts = []
    for _ in range(r.randint(1, 5)):
        x = r.random()
        if x < 0.4:
            parts.append(r.choice(LETTER_TRAPS))
        elif x < 0.7:
            parts.append(str(r.randint(0, 99999)))
        else:
            parts.append("".join(r.choice(sorted(CHARS - {" "})) for _ in range(r.randint(1, 6))))
    return " ".join(parts)


def sample_text(r, books):
    """One label: 50 % book text, 25 % game text, 10 % look-alike traps, with case changes."""
    x = r.random()
    if x < 0.5:
        s = book_line(r, books, r.randint(3, 34))
    elif x < 0.75:
        s = game_line(r, books)
    elif x < 0.85:
        s = trap_line(r)
    else:
        s = " ".join(word(r, books) for _ in range(r.randint(1, 3)))
    y = r.random()
    if y < 0.25:  # games and comics use many all-caps lines
        s = s.upper()
    elif y < 0.26:
        s = s.title()
    s = clean(s)[:MAX_LEN].strip()
    return s or "OK"


# ------------------------------------------------------------------------------------------------- fonts

@lru_cache(maxsize=None)
def native_pixel_size(path: str) -> int:
    """The pixel size at which a pixel font's outlines fall on whole pixels, or 0 when there is none.

    A pixel font draws every glyph from squares of one size: all outline coordinates are a common offset plus a
    multiple of that step (in font units), so the step is the gcd of the differences between coordinates. Each
    glyph gives its own step (one odd glyph must not spoil the rest); the most common one wins. unitsPerEm / step is
    then the size in pixels at which one square is one screen pixel.
    """
    try:
        f = TTFont(path, lazy=True)
        upem = f["head"].unitsPerEm
        gs = f.getGlyphSet()
        cmap = f.getBestCmap()
        votes = {}
        for ch in "AaBbgHIl0123MWxyz":
            g = cmap.get(ord(ch))
            if g is None:
                continue
            pen = RecordingPen()
            gs[g].draw(pen)
            xs = [int(round(p[0])) for _, pts in pen.value for p in pts]
            ys = [int(round(p[1])) for _, pts in pen.value for p in pts]
            if len(xs) < 4:
                continue
            step = 0
            for vals in (xs, ys):
                for v in vals:
                    step = math.gcd(step, abs(v - vals[0]))
            if step:
                votes[step] = votes.get(step, 0) + 1
        if not votes:
            return 0
        step = max(votes, key=lambda s: votes[s])
        px = upem / step
        return int(round(px)) if 5 <= px <= 40 else 0
    except Exception:
        return 0


@lru_cache(maxsize=None)
def caps_only(path: str) -> bool:
    """True for a font that draws lower-case letters as capitals (Bangers, Cinzel, small-caps cuts).

    Such a font must get an upper-case label: the image shows capitals, and a label in lower case would teach the
    model to read capitals as lower case. Each lower-case letter is drawn, scaled to the size of its capital and
    compared; most of them matching means the font has no lower case of its own.
    """
    if os.path.basename(path).startswith(CAPS_FONTS):
        return True
    try:
        font = ImageFont.truetype(path, 48)
        same = 0
        letters = "aegmrst"
        for c in letters:
            lo, up = font.getmask(c), font.getmask(c.upper())
            if lo.getbbox() is None or up.getbbox() is None:
                continue
            a = np.asarray(Image.frombytes("L", lo.size, bytes(lo)).crop(lo.getbbox()).resize((32, 32)), np.float32) > 127
            b = np.asarray(Image.frombytes("L", up.size, bytes(up)).crop(up.getbbox()).resize((32, 32)), np.float32) > 127
            union = np.logical_or(a, b).sum()
            if union and np.logical_and(a, b).sum() / union > 0.8:
                same += 1
        return same >= len(letters) - 2
    except Exception:
        return False


def font_has_all(font: ImageFont.FreeTypeFont, text: str) -> bool:
    """False when a character would come out as a missing-glyph box (its mask equals the box of a private char)."""
    try:
        missing = font.getmask("").getbbox()
    except Exception:
        missing = None
    for c in set(text) - {" "}:
        m = font.getmask(c)
        if m.getbbox() is None or (missing is not None and m.getbbox() == missing and c != ""):
            return False
    return True


# ------------------------------------------------------------------------------------------------- drawing

def colours(r):
    """(text, background, outline) as RGB; dark backgrounds are more common, as in games."""
    def rnd(lo, hi):
        return tuple(r.randint(lo, hi) for _ in range(3))
    x = r.random()
    if x < 0.35:
        bg = (0, 0, 0)
        fg = r.choice([(255, 255, 255), (255, 255, 0), rnd(150, 255)])
    elif x < 0.6:
        bg = rnd(0, 90)
        fg = rnd(170, 255)
    elif x < 0.85:
        bg = rnd(170, 255)
        fg = rnd(0, 80)
    else:
        bg = rnd(0, 255)
        lum = sum(bg) / 3
        fg = rnd(0, 60) if lum > 128 else rnd(195, 255)
    outline = (0, 0, 0) if sum(fg) > 384 else (255, 255, 255)
    return fg, bg, outline


def background(r, size, bg):
    """A plain, gradient or noisy background of `size`."""
    w, h = size
    img = np.empty((h, w, 3), np.float32)
    img[:] = bg
    x = r.random()
    if x < 0.2:  # vertical gradient
        d = np.linspace(-1, 1, h, dtype=np.float32)[:, None, None] * r.uniform(10, 50)
        img += d
    elif x < 0.35:  # noise
        img += np.random.default_rng(r.randrange(1 << 30)).normal(0, r.uniform(3, 18), img.shape)
    return np.clip(img, 0, 255).astype(np.uint8)


def compose(r, mask: Image.Image, fg, bg, outline, effect) -> np.ndarray:
    """Colours a 0/255 text mask onto a background, with an optional outline or shadow drawn from the mask."""
    m = np.asarray(mask, np.float32) / 255.0
    img = background(r, mask.size, bg).astype(np.float32)
    if effect == "outline":
        o = np.asarray(mask.filter(ImageFilter.MaxFilter(3)), np.float32) / 255.0
        img = img * (1 - o[..., None]) + np.array(outline, np.float32) * o[..., None]
    elif effect == "shadow":
        s = np.roll(np.roll(m, 1, 0), 1, 1)
        img = img * (1 - s[..., None]) + np.array(outline, np.float32) * s[..., None]
    img = img * (1 - m[..., None]) + np.array(fg, np.float32) * m[..., None]
    return np.clip(img, 0, 255).astype(np.uint8)


def text_mask(font, text, antialias: bool, pad: int):
    """The text drawn white on black, `pad` pixels round its ink; None when nothing is drawn."""
    left, top, right, bottom = font.getbbox(text)
    w, h = right - left + 2 * pad, bottom - top + 2 * pad
    if w <= 2 * pad or h <= 2 * pad or w > 6000:
        return None
    img = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(img)
    d.fontmode = "L" if antialias else "1"
    d.text((pad - left, pad - top), text, fill=255, font=font)
    return img


def crop_like_detector(r, img: np.ndarray, ink_h: int) -> np.ndarray:
    """Cuts round the ink with the loose, uneven margins of the detector's boxes."""
    # The ink box is known: the mask was drawn with the same pad on every side.
    h, w = img.shape[:2]
    pad = (h - ink_h) // 2
    # The detector expands its boxes (unclip), so Lexia's crops have room round the ink, not always the same.
    top = max(0, pad - int(ink_h * r.uniform(0.1, 0.45)))
    bottom = min(h, h - pad + int(ink_h * r.uniform(0.1, 0.45)))
    left = max(0, pad - int(ink_h * r.uniform(0.1, 0.6)))
    right = min(w, w - pad + int(ink_h * r.uniform(0.1, 0.6)))
    return img[top:bottom, left:right]


def lexia_resize(img: np.ndarray):
    """src/ocr.cpp Recognize: height 48, width rounded up to a multiple of 4, cubic when growing, area when shrinking."""
    h, w = img.shape[:2]
    W = int(round(H * w / h))
    W = min(4096, max(16, (W + 3) // 4 * 4))
    interp = cv2.INTER_CUBIC if H > h else cv2.INTER_AREA
    return cv2.resize(img, (W, H), interpolation=interp)


def render_pixel(r, path, text):
    """A pixel font at its own size (or a small size if it has none), no anti-aliasing, scaled up like a game."""
    native = native_pixel_size(path)
    # A font with no pixel grid of its own (dot-matrix, variable) is drawn big enough to stay readable.
    size = native * r.choice([1, 1, 1, 2]) if native else r.choice([12, 14, 16, 18, 20, 24])
    font = ImageFont.truetype(path, size)
    if not font_has_all(font, text):
        return None
    mask = text_mask(font, text, antialias=False, pad=max(4, size))
    if mask is None:
        return None
    ink_h = mask.size[1] - 2 * max(4, size)
    fg, bg, outline = colours(r)
    effect = r.choices(["none", "outline", "shadow"], [0.7, 0.15, 0.15])[0]
    img = compose(r, mask, fg, bg, outline, effect)
    # The game's screen scaled to the monitor: a whole factor or, as in a window or full screen, any factor.
    f = r.choice([1, 2, 2, 3, 3, 4]) if r.random() < 0.6 else r.uniform(1.3, 4.5)
    filt = cv2.INTER_NEAREST if r.random() < 0.85 else cv2.INTER_LINEAR
    img = cv2.resize(img, (max(1, int(img.shape[1] * f)), max(1, int(img.shape[0] * f))), interpolation=filt)
    return crop_like_detector(r, img, int(ink_h * f))


def render_normal(r, path, text):
    """A normal font, anti-aliased, at a screen size; sometimes a light blur or JPEG loss as in a video."""
    size = r.randint(11, 44)
    font = ImageFont.truetype(path, size)
    try:  # a variable font: a random weight
        axes = font.get_variation_axes()
        font.set_variation_by_axes([r.uniform(a["minimum"], a["maximum"]) if a.get("name", b"") in (b"Weight", "Weight")
                                    else a["default"] for a in axes])
    except Exception:
        pass
    if not font_has_all(font, text):
        return None
    pad = max(6, size // 2)
    mask = text_mask(font, text, antialias=True, pad=pad)
    if mask is None:
        return None
    ink_h = mask.size[1] - 2 * pad
    fg, bg, outline = colours(r)
    effect = r.choices(["none", "outline", "shadow"], [0.8, 0.1, 0.1])[0]
    img = compose(r, mask, fg, bg, outline, effect)
    if r.random() < 0.12:
        img = cv2.GaussianBlur(img, (3, 3), r.uniform(0.3, 0.9))
    if r.random() < 0.15:
        ok, enc = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, r.randint(55, 92)])
        if ok:
            img = cv2.imdecode(enc, cv2.IMREAD_COLOR)
    return crop_like_detector(r, img, ink_h)


# ------------------------------------------------------------------------------------------------- workers

_STATE = {}


def _init(manifest_path, books):
    with open(manifest_path, encoding="utf-8") as f:
        m = json.load(f)
    _STATE["pixel"] = [p for fam in m["fonts"] if fam["kind"] == "pixel" for p in fam["files"]]
    _STATE["normal"] = [p for fam in m["fonts"] if fam["kind"] == "normal" for p in fam["files"]]
    _STATE["books"] = books
    cv2.setNumThreads(1)


def make_one(job):
    """Renders sample `index` of a split; returns (relative path, label, kind) or None after 8 failed tries."""
    split, index, seed, out_dir, kind = job
    r = random.Random(seed * 1_000_003 + index)
    if kind is None:
        kind = "pixel" if r.random() < PIXEL_SHARE else "normal"
    for _ in range(8):
        text = sample_text(r, _STATE["books"])
        path = r.choice(_STATE[kind])
        if caps_only(path):
            text = text.upper()  # the label says what the image shows
        try:
            img = render_pixel(r, path, text) if kind == "pixel" else render_normal(r, path, text)
        except Exception:
            img = None
        if img is None or img.shape[0] < 4 or img.shape[1] < 4:
            continue
        img = lexia_resize(img)
        if img.shape[1] > MAX_W:
            # too long for the training width: shorten the text and try again
            continue
        rel = f"{split}/{index:07d}.png"
        # PNG keeps the pixels exact; RGB on disk, read back as BGR by PaddleOCR like Lexia's screen BGR
        cv2.imwrite(os.path.join(out_dir, rel), img[:, :, ::-1])
        return rel, text, kind
    return None


def run_split(pool, out_dir, split, count, seed, kind):
    os.makedirs(os.path.join(out_dir, split), exist_ok=True)
    jobs = [(split, i, seed, out_dir, kind) for i in range(count)]
    rows = []
    for k, res in enumerate(pool.imap(make_one, jobs, chunksize=256)):
        if res is not None:
            rows.append(res)
        if (k + 1) % 20000 == 0:
            print(f"  {split}: {k + 1} / {count}", flush=True)
    return rows


def write_list(path, rows):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for rel, text, _ in rows:
            f.write(f"{rel}\t{text}\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", required=True)
    ap.add_argument("--train", type=int, default=400000)
    ap.add_argument("--val", type=int, default=2000, help="lines per validation set (pixel and normal)")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--out", help="output folder (default <work>\\data); a small trial run can use another")
    ap.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    args = ap.parse_args()

    work = os.path.abspath(args.work)
    manifest = os.path.join(work, "sources", "manifest.json")
    with open(manifest, encoding="utf-8") as f:
        books = load_books(json.load(f)["texts"])
    out = os.path.abspath(args.out) if args.out else os.path.join(work, "data")
    os.makedirs(out, exist_ok=True)
    print(f"{len(books)} book words; writing to {out} with {args.workers} workers", flush=True)

    with mp.Pool(args.workers, initializer=_init, initargs=(manifest, books)) as pool:
        # other seeds for the check sets, so no check image is a training image
        vp = run_split(pool, out, "val_pixel", args.val, args.seed + 101, "pixel")
        vn = run_split(pool, out, "val_normal", args.val, args.seed + 202, "normal")
        tr = run_split(pool, out, "train", args.train, args.seed, None)
    write_list(os.path.join(out, "val_pixel.txt"), vp)
    write_list(os.path.join(out, "val_normal.txt"), vn)
    write_list(os.path.join(out, "val_list.txt"), vp + vn)
    write_list(os.path.join(out, "train_list.txt"), tr)
    pix = sum(1 for row in tr if row[2] == "pixel")
    print(f"done: {len(tr)} training lines ({pix} pixel, {len(tr) - pix} normal), {len(vp)} + {len(vn)} check lines")


if __name__ == "__main__":
    sys.exit(main())
