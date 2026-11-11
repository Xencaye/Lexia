r"""Makes the synthetic training lines for rec_en.onnx (task L): pixel game fonts mixed with normal fonts.

    python -I gen_lines.py --work <dir> [--train 400000] [--val 2000] [--seed 7] [--workers N]

Reads <work>\sources\manifest.json (written by fetch_sources.py) and writes PaddleOCR SimpleDataSet files:
    <work>\data\train\*.png, <work>\data\val_pixel\*.png, <work>\data\val_normal\*.png
    <work>\data\train_list.txt, val_pixel.txt, val_normal.txt, val_list.txt   ("relative/path.png<TAB>label")

What it keeps true:
- Every image is already what Lexia gives the recogniser: the crop resized to height 48 with INTER_CUBIC when it
  grows and INTER_AREA when it shrinks, the width rounded up to a multiple of 4 (src/ocr.cpp, Recognize). The model
  then learns the pixels Lexia really feeds it (the width rounded half away from zero, as std::lround). A line wider
  than MAX_W after that resize loses its last word until it fits, so long lines stay in the data.
- Every label holds only characters of dict_en.txt (plus the space), at most MAX_LEN of them, no leading, trailing
  or double spaces; the one exception is the crops with no text (NEGATIVE_SHARE), whose label is a single space.
- One space in a label for each word gap in the image, however wide the gap is (tracking, word_gap), and none
  inside a word.
- The same --seed gives the same lines: sample i draws from its own random.Random(seed * 1_000_003 + i), so the
  number of workers does not change the data.
- Pixel fonts are drawn at their own pixel size without anti-aliasing and scaled up with nearest neighbour, as a
  game does on a big screen; normal fonts are drawn anti-aliased at screen sizes.
- The validation sets use other seeds than the training set, so the check in check_onnx.py measures reading, not
  memory. No label in any set equals a line of a Fixture's expected.txt (load_fixture_lines), so the Fixture score
  is not memory either; the game templates still use the words of game menus, so a Fixture line can be close.
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
PIXEL_SHARE = 0.4      # of the text lines
# Training crops with no text, labelled with one space (render_negative). PaddleOCR drops an empty label, but keeps
# " " (SimpleDataSet strips only the line end); Lexia drops a line with no letter or digit, so " " means "nothing".
# Kept small: a model that answers " " too readily would also drop faint real text.
NEGATIVE_SHARE = 0.04
# Caps-only fonts whose small capitals differ too much from the capitals for caps_only() to see it.
CAPS_FONTS = ("Cinzel",)

# ------------------------------------------------------------------------------------------------- characters

with open(DICT_FILE, encoding="utf-8") as _f:
    CHARS = set(line.rstrip("\r\n") for line in _f if line.rstrip("\r\n"))
CHARS.add(" ")

# Typographic characters in the books mapped to the plain ones of the dictionary. "_" is a dictionary character
# and stays: the books' _italic_ markers are removed in load_books, not here, so user names keep theirs.
REPLACE = {"‘": "'", "’": "'", "“": '"', "”": '"', "—": "-", "–": "-", "…": "...",
           " ": " "}


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
        # Project Gutenberg marks italics as _word_: those underscores are markup, not text.
        words.extend(clean(t.replace("\n", " ").replace("_", " ")).split(" "))
    return [w for w in words if w]


def load_fixture_lines(fixtures_dir):
    """Every line of the Fixtures' expected.txt files, case-folded, so make_one can refuse them as labels.

    The Fixtures measure how Lexia reads screens it was not trained on (score_layout.ps1); a label equal to one of
    their lines would make that score partly memory. The templates still use game words (Confirm, Cancel, HP), so a
    Fixture line can be close to a label, never equal. Missing folder: no lines (a copy without tests).
    """
    lines = set()
    if os.path.isdir(fixtures_dir):
        for name in sorted(os.listdir(fixtures_dir)):
            p = os.path.join(fixtures_dir, name, "expected.txt")
            if os.path.isfile(p):
                with open(p, encoding="utf-8", errors="replace") as f:
                    lines.update(s.strip().casefold() for s in f if s.strip())
    return lines


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
                "1l", "il", "Ill", "lll", "o0", "Q0", "U", "u", "V", "v", "Y", "y", "C", "c", "K", "k",
                # Runs of the same letter: CTC needs a blank between two equal classes, and a pixel font's equal
                # columns make one of them easy to lose ("Chapter III" read as "Chapter II" in round 1).
                "II", "III", "IIII", "XIII", "VIII", "MM", "WW", "mm", "nn", "rr", "ss", "ee", "oo", "tt", "ff", "11",
                "111", "000", "00", "...", "!!", "??"]
ROMAN = ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XX"]
NAME_PARTS = ["angel", "glitch", "pixel", "cream", "moon", "star", "core", "blue", "dark", "neo", "byte", "frog",
              "OS", "cat", "fox", "sky", "nova", "ghost", "kiwi", "void", "rain", "echo", "lily", "max", "zero"]


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
    t = r.randrange(18)
    if t == 14:  # chapter titles with Roman numerals: runs of I, V and X
        return f"{r.choice(['Chapter', 'Part', 'Act', 'Book', 'Episode', 'CHAPTER', 'PART'])} {r.choice(ROMAN)}, {book_line(r, books, 20)}"
    if t == 15:  # version lines as on title screens
        return (f"{r.choice(['', 'Early Access ', 'Demo ', 'Beta '])}Version "
                f"{r.randint(0, 3)}.{r.randint(0, 20)}.{r.randint(0, 40)}")
    if t == 16:  # user names and file names of chat and desktop-style games (the only source of "_")
        sep = r.choice(["_", "_", "-", ".", ""])
        name = sep.join(r.choice(NAME_PARTS) for _ in range(r.randint(2, 3)))
        return r.choice([name, f"{name}.exe", f"{name}{r.randint(0, 99)}", f"@{name}"])
    if t == 17:  # dialogue with the speaker's name
        return f"{r.choice(NAME_PARTS).title()}: {book_line(r, books, 28)}"
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


def clouds(r, w, h, amp):
    """A soft, cloud-like texture (-amp..amp): coarse noise blown up with cubic interpolation, as in painted or
    photographed game backgrounds."""
    rng = np.random.default_rng(r.randrange(1 << 30))
    gw, gh = max(2, w // r.randint(8, 40)), max(2, h // r.randint(6, 30))
    small = rng.uniform(-1, 1, (gh, gw, 3)).astype(np.float32)
    if r.random() < 0.6:  # mostly brightness, not colour
        small[:] = small[..., :1]
    return cv2.resize(small, (w, h), interpolation=cv2.INTER_CUBIC) * amp


def clutter(r, img, base, n, contrast):
    """Draws n random shapes (circles, boxes, diagonal or thick strokes, crosses) on img, each in a colour at most
    `contrast` away from `base`.

    No lone thin vertical or horizontal stroke is drawn: on its own it looks like "l", "I", "|", "-" or "_", and the
    model must not learn to see nothing (or something) where a real one stands. A hollow box has thin sides, but as
    a closed frame round the text area (or cut by the crop) it reads as a panel edge, as in game menus.
    """
    h, w = img.shape[:2]
    for _ in range(n):
        col = tuple(float(np.clip(c + r.uniform(-contrast, contrast), 0, 255)) for c in base)
        kind = r.randrange(5)
        cx, cy = r.randrange(w), r.randrange(h)
        s = max(2, int(h * r.uniform(0.15, 0.9)))
        if kind == 0:
            cv2.circle(img, (cx, cy), s // 2, col, -1 if r.random() < 0.6 else max(1, s // 8), cv2.LINE_AA)
        elif kind == 1:
            cv2.rectangle(img, (cx - s, cy - s // 2), (cx + s, cy + s // 2), col, -1 if r.random() < 0.5 else max(1, s // 8))
        elif kind == 2:  # a diagonal stroke, 20 to 70 degrees
            a = math.radians(r.uniform(20, 70)) * r.choice([1, -1])
            dx, dy = int(math.cos(a) * s), int(math.sin(a) * s)
            cv2.line(img, (cx - dx, cy - dy), (cx + dx, cy + dy), col, max(1, s // 6), cv2.LINE_AA)
        elif kind == 3:  # a cross, as the "X" decorations of lovebyte-style screens
            d = s // 2
            t = max(1, s // 8)
            cv2.line(img, (cx - d, cy - d), (cx + d, cy + d), col, t, cv2.LINE_AA)
            cv2.line(img, (cx - d, cy + d), (cx + d, cy - d), col, t, cv2.LINE_AA)
        else:  # a band thick enough not to read as a dash
            t = max(3, int(h * r.uniform(0.3, 0.6)))
            cv2.line(img, (0, cy), (w, cy), col, t)
    return img


def background(r, size, bg, fg=None):
    """A plain, gradient, noisy, cloudy or cluttered background of `size`.

    Clutter keeps away from the text colour `fg` (when given) so the text stays the clearest thing in the crop.
    """
    w, h = size
    img = np.empty((h, w, 3), np.float32)
    img[:] = bg
    x = r.random()
    if x < 0.2:  # vertical gradient
        d = np.linspace(-1, 1, h, dtype=np.float32)[:, None, None] * r.uniform(10, 50)
        img += d
    elif x < 0.32:  # noise
        img += np.random.default_rng(r.randrange(1 << 30)).normal(0, r.uniform(3, 18), img.shape)
    elif x < 0.47:  # clouds, as behind the title of a game
        img += clouds(r, w, h, r.uniform(15, 60))
    elif x < 0.57:  # faint shapes behind the text
        lum_gap = abs(sum(bg) - sum(fg)) / 3 if fg is not None else 120
        img = clutter(r, img, bg, r.randint(1, 4), min(70, lum_gap * 0.45))
    return np.clip(img, 0, 255).astype(np.uint8)


def compose(r, mask: Image.Image, fg, bg, outline, effect) -> np.ndarray:
    """Colours a 0/255 text mask onto a background, with an optional outline or shadow drawn from the mask.

    "thick" is the comic and title style: an outline several pixels wide (the filter is 6 to 16 % of the mask's
    height, padding included, and odd so it is centred), as on the lettering of wha_01.
    """
    m = np.asarray(mask, np.float32) / 255.0
    img = background(r, mask.size, bg, fg).astype(np.float32)
    if effect in ("outline", "thick"):
        k = 3
        if effect == "thick":
            k = 2 * max(1, int(mask.size[1] * r.uniform(0.03, 0.08))) + 1
        o = np.asarray(mask.filter(ImageFilter.MaxFilter(k)), np.float32) / 255.0
        img = img * (1 - o[..., None]) + np.array(outline, np.float32) * o[..., None]
    elif effect == "shadow":
        s = np.roll(np.roll(m, 1, 0), 1, 1)
        img = img * (1 - s[..., None]) + np.array(outline, np.float32) * s[..., None]
    img = img * (1 - m[..., None]) + np.array(fg, np.float32) * m[..., None]
    return np.clip(img, 0, 255).astype(np.uint8)


def text_mask(font, text, antialias: bool, pad: int, tracking: int = 0, word_gap: int = 0):
    """The text drawn white on black, `pad` pixels round its ink; None when nothing is drawn.

    `tracking` (pixels added after every character) and `word_gap` (added to every space) draw the text one
    character at a time, for wide monospace UIs and spaced-out titles: the gaps are then wider than the font's own,
    and the label still has one space per word gap and none inside a word ("Early Access Version" in round 1 came
    out as "Early A Access Version0.9.18"). Kerning is lost in that mode, which such text has not got either.
    """
    if tracking == 0 and word_gap == 0:
        left, top, right, bottom = font.getbbox(text)
        xs = None
    else:
        xs, x = [], 0.0
        for c in text:
            xs.append(int(round(x)))
            x += font.getlength(c) + tracking + (word_gap if c == " " else 0)
        boxes = [(xs[i] + b[0], b[1], xs[i] + b[2], b[3])
                 for i, c in enumerate(text) if c != " " for b in [font.getbbox(c)]]
        if not boxes:
            return None
        left, top = min(b[0] for b in boxes), min(b[1] for b in boxes)
        right, bottom = max(b[2] for b in boxes), max(b[3] for b in boxes)
    w, h = right - left + 2 * pad, bottom - top + 2 * pad
    if w <= 2 * pad or h <= 2 * pad or w > 6000:
        return None
    img = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(img)
    d.fontmode = "L" if antialias else "1"
    if xs is None:
        d.text((pad - left, pad - top), text, fill=255, font=font)
    else:
        for c, cx in zip(text, xs):
            if c != " ":
                d.text((pad - left + cx, pad - top), c, fill=255, font=font)
    return img


def camera(r, img: np.ndarray) -> np.ndarray:
    """A screen photographed or filmed with a phone (wha_01): a small tilt, detail lost to a lower resolution,
    blur, sensor noise, a colour cast with less contrast, and strong JPEG loss."""
    h, w = img.shape[:2]
    if r.random() < 0.6:
        m = cv2.getRotationMatrix2D((w / 2, h / 2), r.uniform(-3, 3), 1.0)
        img = cv2.warpAffine(img, m, (w, h), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE)
    f = r.uniform(0.4, 0.8)
    small = cv2.resize(img, (max(1, int(w * f)), max(1, int(h * f))), interpolation=cv2.INTER_AREA)
    img = cv2.resize(small, (w, h), interpolation=cv2.INTER_LINEAR)
    img = cv2.GaussianBlur(img, (0, 0), r.uniform(0.4, 1.2)).astype(np.float32)
    img += np.random.default_rng(r.randrange(1 << 30)).normal(0, r.uniform(2, 10), img.shape)
    gain = np.array([r.uniform(0.7, 1.0) for _ in range(3)], np.float32)
    img = img * gain + r.uniform(0, 50)
    img = np.clip(img, 0, 255).astype(np.uint8)
    ok, enc = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, r.randint(35, 75)])
    return cv2.imdecode(enc, cv2.IMREAD_COLOR) if ok else img


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
    # std::lround rounds a half away from zero; Python's round() rounds it to even (24.5 -> 24, not 25), which
    # would give some crops another width than Lexia gives them. The ratio is positive, so floor(x + 0.5) matches.
    W = int(math.floor(H * w / h + 0.5))
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
    # Whole pixels at the font's own size: the extra gap stays on the pixel grid after the nearest-neighbour scale.
    unit = max(1, size // max(1, native or 8))
    tracking = unit * r.randint(1, 3) if r.random() < 0.25 else 0
    word_gap = unit * r.randint(2, 8) if r.random() < 0.15 else 0
    word_gap = max(word_gap, tracking)  # a word gap stays clearly wider than the gap between two letters
    mask = text_mask(font, text, antialias=False, pad=max(4, size), tracking=tracking, word_gap=word_gap)
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
    if r.random() < 0.03:  # a game filmed off a screen
        img = camera(r, img)
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
    # Spaced-out titles and wide monospace UIs: extra room after each character or at each space.
    tracking = int(size * r.uniform(0.05, 0.4)) if r.random() < 0.2 else 0
    word_gap = int(size * r.uniform(0.3, 1.5)) if r.random() < 0.15 else 0
    word_gap = max(word_gap, tracking)  # a word gap stays clearly wider than the gap between two letters
    mask = text_mask(font, text, antialias=True, pad=pad, tracking=tracking, word_gap=word_gap)
    if mask is None:
        return None
    ink_h = mask.size[1] - 2 * pad
    fg, bg, outline = colours(r)
    effect = r.choices(["none", "outline", "thick", "shadow"], [0.7, 0.08, 0.12, 0.1])[0]
    img = compose(r, mask, fg, bg, outline, effect)
    if r.random() < 0.07:  # a screen filmed with a phone: replaces the lighter blur and JPEG below
        return crop_like_detector(r, camera(r, img), ink_h)
    if r.random() < 0.12:
        img = cv2.GaussianBlur(img, (3, 3), r.uniform(0.3, 0.9))
    if r.random() < 0.15:
        ok, enc = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, r.randint(55, 92)])
        if ok:
            img = cv2.imdecode(enc, cv2.IMREAD_COLOR)
    return crop_like_detector(r, img, ink_h)


def pixel_sprite(r):
    """A small mirror-symmetric pixel icon (as space-invader sprites and UI glyphs are), 0/1 values.

    Dense enough (about half the cells) that it reads as a blob, not as a thin letter."""
    n = r.randint(5, 9)
    half = (n + 1) // 2
    left = np.array([[1 if r.random() < 0.55 else 0 for _ in range(half)] for _ in range(n)], np.uint8)
    return np.concatenate([left, left[:, : n // 2][:, ::-1]], axis=1)


def render_negative(r):
    """A crop with no text in it, labelled " " (see NEGATIVE_SHARE): a background with shapes, icons or nothing.

    Lexia's detector also boxes icons, decorations and textures; a model that never saw such a crop reads letters
    into it ("wwww", "w*", "z777777" in round 1). Lexia drops a line with no letter or digit (src/ocr.cpp), so the
    model's answer " " removes the line. Sizes cover the detector's boxes: small icons to big picture areas.
    """
    h = int(r.uniform(12, 40) if r.random() < 0.6 else r.uniform(40, 170))
    w = max(8, int(h * r.uniform(0.4, 5.0)))
    fg, bg, _ = colours(r)
    img = background(r, (w, h), bg).astype(np.float32)
    x = r.random()
    if x < 0.45:  # shapes in a clear colour
        img = clutter(r, img, bg, r.randint(1, 5), 200)
    elif x < 0.75:  # one to three pixel icons, scaled up with nearest neighbour
        for _ in range(r.randint(1, 3)):
            spr = pixel_sprite(r)
            s = max(1, int(h * r.uniform(0.4, 0.9)) // spr.shape[0])
            big = cv2.resize(spr, (spr.shape[1] * s, spr.shape[0] * s), interpolation=cv2.INTER_NEAREST)
            bh, bw = big.shape
            if bh >= h or bw >= w:
                continue
            y0, x0 = r.randrange(h - bh), r.randrange(w - bw)
            region = img[y0:y0 + bh, x0:x0 + bw]
            region[big > 0] = fg
    # else: only the background (a texture, a gradient, a plain panel)
    img = np.clip(img, 0, 255).astype(np.uint8)
    if r.random() < 0.1:
        img = camera(r, img)
    return img


# ------------------------------------------------------------------------------------------------- workers

_STATE = {}


def _init(manifest_path, books, fixture_lines):
    """Pool worker set-up: the font lists, the book words and the Fixture lines to refuse."""
    with open(manifest_path, encoding="utf-8") as f:
        m = json.load(f)
    _STATE["pixel"] = [p for fam in m["fonts"] if fam["kind"] == "pixel" for p in fam["files"]]
    _STATE["normal"] = [p for fam in m["fonts"] if fam["kind"] == "normal" for p in fam["files"]]
    _STATE["books"] = books
    _STATE["fixtures"] = fixture_lines
    cv2.setNumThreads(1)


def make_one(job):
    """Renders sample `index` of a split; returns (relative path, label, kind) or None after 8 failed tries."""
    split, index, seed, out_dir, kind = job
    r = random.Random(seed * 1_000_003 + index)
    if kind is None:
        x = r.random()
        if x < NEGATIVE_SHARE:
            kind = "negative"
        else:
            kind = "pixel" if r.random() < PIXEL_SHARE else "normal"
    if kind == "negative":
        img = lexia_resize(render_negative(r))[:, :MAX_W]
        rel = f"{split}/{index:07d}.png"
        cv2.imwrite(os.path.join(out_dir, rel), img[:, :, ::-1])
        return rel, " ", kind
    for _ in range(8):
        text = sample_text(r, _STATE["books"])
        path = r.choice(_STATE[kind])
        if caps_only(path):
            text = text.upper()  # the label says what the image shows
        # Too wide for the training width: drop the last word and draw again in the same font, so long lines stay
        # in the data (a new text would favour short ones). A Fixture line is never a label (load_fixture_lines).
        while True:
            if text.casefold() in _STATE["fixtures"]:
                img = None
                break
            try:
                img = render_pixel(r, path, text) if kind == "pixel" else render_normal(r, path, text)
            except Exception:
                img = None
            if img is None or img.shape[0] < 4 or img.shape[1] < 4:
                img = None
                break
            img = lexia_resize(img)
            if img.shape[1] <= MAX_W or " " not in text:
                break
            text = text.rsplit(" ", 1)[0].rstrip()
        if img is None or img.shape[1] > MAX_W:
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
    fixture_lines = load_fixture_lines(os.path.join(HERE, "..", "..", "tests", "fixtures"))
    print(f"{len(books)} book words, {len(fixture_lines)} Fixture lines refused; writing to {out} with "
          f"{args.workers} workers", flush=True)

    with mp.Pool(args.workers, initializer=_init, initargs=(manifest, books, fixture_lines)) as pool:
        # other seeds for the check sets, so no check image is a training image
        vp = run_split(pool, out, "val_pixel", args.val, args.seed + 101, "pixel")
        vn = run_split(pool, out, "val_normal", args.val, args.seed + 202, "normal")
        tr = run_split(pool, out, "train", args.train, args.seed, None)
    write_list(os.path.join(out, "val_pixel.txt"), vp)
    write_list(os.path.join(out, "val_normal.txt"), vn)
    write_list(os.path.join(out, "val_list.txt"), vp + vn)
    write_list(os.path.join(out, "train_list.txt"), tr)
    pix = sum(1 for row in tr if row[2] == "pixel")
    neg = sum(1 for row in tr if row[2] == "negative")
    print(f"done: {len(tr)} training lines ({pix} pixel, {len(tr) - pix - neg} normal, {neg} with no text), "
          f"{len(vp)} + {len(vn)} check lines")


if __name__ == "__main__":
    sys.exit(main())
