r"""Downloads the fonts and texts that the rec_en training lines are drawn from (task L).

    python -I fetch_sources.py --work <dir>          # download, check every file against sources.lock.json
    python -I fetch_sources.py --work <dir> --pin    # first time only: write the hashes into sources.lock.json

Rules this script keeps:
- Every download is checked against the SHA-256 in sources.lock.json; a mismatch stops the run. --pin is the only
  way a hash gets into the lock file, and its diff is reviewed in the PR like any pin in fetch_deps.ps1.
- A font is used only when the licence file that comes with it contains `licenceMustContain` (a font site's label is
  not enough: docs/notes/training-fonts.md). For a Google Fonts family that is the OFL.txt (or UFL.txt) next to it.
- Archives are unpacked member by member: a name with "..", an absolute path or a drive letter is refused, links are
  skipped, so an archive cannot write outside its own folder.
- Everything goes under <work>\sources (outside the repo and OneDrive); nothing here is committed. Each good
  download is kept in <work>\sources\cache under its SHA-256, so a second run downloads only what is new.
Run it with `python -I` so that no file in the download folders can be imported.
"""

import argparse
import fnmatch
import hashlib
import io
import json
import os
import shutil
import sys
import tarfile
import urllib.parse
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCES = os.path.join(HERE, "sources.json")
LOCK = os.path.join(HERE, "sources.lock.json")
# A plain client name: no user details in requests to outside services.
USER_AGENT = "lexia-training-fetch/1.0"
MAX_BYTES = 64 * 1024 * 1024  # no font or book is near this; a larger answer is refused


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def download(url: str) -> bytes:
    """Fetches one URL over HTTPS (redirects followed) into memory, at most MAX_BYTES."""
    if not url.startswith("https://"):
        raise SystemExit(f"refused: not HTTPS: {url}")
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=60) as r:
        data = r.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise SystemExit(f"refused: larger than {MAX_BYTES} bytes: {url}")
    return data


# Folder of earlier downloads, named by their SHA-256 (set by main): a run after a failed or finished one reads a
# pinned file from here instead of downloading it again.
CACHE = None


def checked(key: str, url: str, lock: dict, pin: bool) -> bytes:
    """Downloads `url` and checks it against lock[key]; with --pin, records the hash of a new key instead.

    A pinned file already in CACHE under its hash is read from there, and used only if its bytes still have that
    hash; anything else is downloaded. Each good download is kept in CACHE.
    """
    want = lock.get(key)
    if CACHE and want:
        cached = os.path.join(CACHE, want)
        if os.path.isfile(cached):
            with open(cached, "rb") as f:
                data = f.read()
            if sha256(data) == want:
                return data
    data = download(url)
    got = sha256(data)
    want = lock.get(key)
    if want is None:
        if not pin:
            raise SystemExit(f"{key}: no hash in sources.lock.json (run once with --pin, then review the diff)")
        lock[key] = got
        print(f"  pinned {key} = {got}")
    elif want != got:
        raise SystemExit(f"{key}: SHA-256 mismatch for {url}\n  expected {want}\n  got      {got}")
    if CACHE:
        write(os.path.join(CACHE, got), data)
    return data


def safe_member(name: str) -> bool:
    """True when an archive member name stays inside the folder it is unpacked to."""
    n = name.replace("\\", "/")
    return not (n.startswith("/") or ":" in n or any(p == ".." for p in n.split("/")))


def unpack(data: bytes, kind: str):
    """Yields (member name, bytes) for each regular file of a zip or tar.gz held in memory."""
    if kind == "zip":
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            for info in z.infolist():
                if info.is_dir() or not safe_member(info.filename):
                    continue
                yield info.filename, z.read(info)
    elif kind == "tar.gz":
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as t:
            for m in t.getmembers():
                if not m.isfile() or not safe_member(m.name):  # links and devices are skipped
                    continue
                fh = t.extractfile(m)
                if fh is not None:
                    yield m.name, fh.read()
    else:
        raise SystemExit(f"unknown archive kind {kind}")


def write(path: str, data: bytes):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def fetch_font(font: dict, commit: str, out_dir: str, lock: dict, pin: bool) -> list:
    """Puts one font family's files into out_dir/<id>/ and returns their paths; stops if the licence check fails."""
    fid = font["id"]
    dest = os.path.join(out_dir, fid)
    shutil.rmtree(dest, ignore_errors=True)
    must = font.get("licenceMustContain", "SIL OPEN FONT LICENSE")
    paths = []
    if "google" in font:
        base = f"https://raw.githubusercontent.com/google/fonts/{commit}/{font['google']}/"
        lic_name = font.get("licenceName", "OFL.txt")
        lic = checked(f"{fid}/{lic_name}", base + lic_name, lock, pin)
        licence_text = lic.decode("utf-8", "replace")
        for name in font["files"]:
            data = checked(f"{fid}/{name}", base + urllib.parse.quote(name), lock, pin)
            p = os.path.join(dest, name)
            write(p, data)
            paths.append(p)
        write(os.path.join(dest, lic_name), lic)
    else:
        data = checked(f"{fid}/archive", font["url"], lock, pin)
        members = list(unpack(data, font["archive"]))
        licence_text = ""
        if "licenceUrl" in font:  # the archive has no licence file: take it from the same release's source
            licence_text = checked(f"{fid}/licence", font["licenceUrl"], lock, pin).decode("utf-8", "replace")
            write(os.path.join(dest, "LICENSE.txt"), licence_text.encode("utf-8"))
        else:
            for name, body in members:
                base = os.path.basename(name)
                if fnmatch.fnmatch(base.upper(), font["licenceFile"].upper()) and not base.lower().endswith((".ttf", ".otf")):
                    licence_text += body.decode("utf-8", "replace")
                    write(os.path.join(dest, "licence", base), body)
        for name, body in members:
            base = os.path.basename(name)
            if base.startswith("._"):  # macOS resource-fork copies in some zips: not fonts
                continue
            if any(fnmatch.fnmatch(base, pat) or fnmatch.fnmatch(name, pat) for pat in font["files"]):
                p = os.path.join(dest, base)
                write(p, body)
                paths.append(p)
    if must.upper() not in licence_text.upper():
        shutil.rmtree(dest, ignore_errors=True)
        raise SystemExit(f"{fid}: the licence file does not contain '{must}': not used (check docs/notes/training-fonts.md)")
    if not paths:
        raise SystemExit(f"{fid}: no font file matched {font['files']}")
    return paths


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", required=True, help="the training work folder (outside the repo and OneDrive)")
    ap.add_argument("--pin", action="store_true", help="record hashes for entries not yet in sources.lock.json")
    args = ap.parse_args()

    with open(SOURCES, encoding="utf-8") as f:
        src = json.load(f)
    lock = {}
    if os.path.exists(LOCK):
        with open(LOCK, encoding="utf-8") as f:
            lock = json.load(f)

    out = os.path.join(os.path.abspath(args.work), "sources")
    global CACHE
    CACHE = os.path.join(out, "cache")
    manifest = {"fonts": [], "texts": []}
    try:
        for font in src["fonts"]:
            print(f"font {font['id']}")
            files = fetch_font(font, src["googleFontsCommit"], os.path.join(out, "fonts"), lock, args.pin)
            manifest["fonts"].append({"id": font["id"], "kind": font["kind"], "files": files})
        for text in src["texts"]:
            print(f"text {text['id']}")
            data = checked(f"text/{text['id']}", text["url"], lock, args.pin)
            p = os.path.join(out, "texts", text["id"] + ".txt")
            write(p, data)
            manifest["texts"].append(p)
    finally:
        if args.pin:  # keep what was pinned so far, even when a later entry fails
            with open(LOCK, "w", encoding="utf-8", newline="\n") as f:
                json.dump(dict(sorted(lock.items())), f, indent=1)
                f.write("\n")
    # gen_lines.py reads this list instead of walking the folders
    write(os.path.join(out, "manifest.json"), json.dumps(manifest, indent=1).encode("utf-8"))
    print(f"done: {len(manifest['fonts'])} font families, {len(manifest['texts'])} texts in {out}")


if __name__ == "__main__":
    sys.exit(main())
