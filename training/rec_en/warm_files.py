r"""Reads every training image once, with many threads, before training starts (train.ps1 calls it).

    python -I warm_files.py <data dir>

Why: on Windows the first open of each small file is slow (about 4 ms; the virus scanner looks at each new file),
and PaddleOCR's loader opens them one by one: the first epoch ran at 60 lines/s instead of 220. Read in parallel
first, all 400 000 files take about 4 minutes and then sit in the file cache (5 GB; a PC with 16 GB or more keeps
them). Run again after a restart; when the files are cached it takes seconds.
"""

import concurrent.futures as cf
import os
import sys
import time


def main():
    data = os.path.abspath(sys.argv[1])
    paths = []
    for name in ("train_list.txt", "val_list.txt"):
        with open(os.path.join(data, name), encoding="utf-8") as f:
            paths += [os.path.join(data, line.split("\t", 1)[0]) for line in f if "\t" in line]
    start = time.time()

    def size(p):
        with open(p, "rb") as fh:
            return len(fh.read())

    with cf.ThreadPoolExecutor(32) as ex:
        total = sum(ex.map(size, paths, chunksize=256))
    print(f"read {len(paths)} files ({total / 2**30:.1f} GB) in {time.time() - start:.0f} s", flush=True)


if __name__ == "__main__":
    main()
