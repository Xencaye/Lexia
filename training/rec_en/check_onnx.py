r"""Checks an exported rec_en.onnx against what Lexia needs, and against the model it replaces (task L).

    python -I check_onnx.py --new <work>\rec_en.onnx --old <repo>\models\rec_en.onnx --data <work>\data

1. Shapes (src/ocr.cpp reads these, by position, not by name): one input, rank 4, 3 channels, height 48, a free
   width; one output [N, T, 97] of softmax scores (each time step adds up to 1). Names are only reported.
2. Reading: both models on val_pixel.txt and val_normal.txt (lines of gen_lines.py with seeds the training never
   saw), decoded the way ctc.cpp does (greedy, repeats merged, blank dropped). Prints the character error rate (CER)
   and the share of lines read exactly.
3. The rule of the plan: on normal fonts the CER may rise by at most 0.5 points; on pixel fonts it must fall.
   Exit 1 if the shapes are wrong or the rule fails.
"""

import argparse
import os
import sys

import cv2
import numpy as np
import onnxruntime as ort

HERE = os.path.dirname(os.path.abspath(__file__))


def load_dict():
    with open(os.path.join(HERE, "dict_en.txt"), encoding="utf-8") as f:
        chars = [line.rstrip("\r\n") for line in f if line.rstrip("\r\n")]
    return ["<blank>"] + chars + [" "]  # PaddleOCR's order: blank, the dictionary, the space


def check_shapes(sess, name) -> bool:
    ins, outs = sess.get_inputs(), sess.get_outputs()
    ok = len(ins) == 1 and len(outs) == 1
    if ok:
        s = ins[0].shape
        ok = len(s) == 4 and s[1] == 3 and s[2] == 48 and not isinstance(s[3], int)
        o = outs[0].shape
        ok = ok and len(o) == 3 and o[2] == 97
    print(f"{name}: input {[(i.name, i.shape) for i in ins]} output {[(o.name, o.shape) for o in outs]} -> "
          f"{'OK' if ok else 'WRONG'}")
    return ok


def decode(probs, table):
    """Greedy CTC as ctc.cpp: best class per step, repeats merged, blanks dropped (no Alphabet mask: English has all)."""
    best = probs.argmax(axis=1)
    out, prev = [], -1
    for k in best:
        if k != prev and k != 0:
            out.append(table[k])
        prev = k
    return "".join(out).strip()


def edit_distance(a: str, b: str) -> int:
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
        prev = cur
    return prev[-1]


def run(sess, data_dir, list_name, table):
    """Returns (CER in %, exact lines in %, first sum-to-one error) over one check list."""
    errs = chars = exact = n = 0
    worst_sum = 0.0
    with open(os.path.join(data_dir, list_name), encoding="utf-8") as f:
        rows = [line.rstrip("\n").split("\t", 1) for line in f if "\t" in line]
    for rel, label in rows:
        img = cv2.imread(os.path.join(data_dir, rel), cv2.IMREAD_COLOR)  # BGR, as Lexia's screen
        x = (img.astype(np.float32) / 127.5 - 1.0).transpose(2, 0, 1)[None]  # Lexia: x / 127.5 - 1
        probs = sess.run(None, {sess.get_inputs()[0].name: x})[0][0]
        worst_sum = max(worst_sum, float(np.abs(probs.sum(axis=1) - 1).max()))
        text = decode(probs, table)
        e = edit_distance(text, label)
        errs += e
        chars += max(1, len(label))
        exact += e == 0
        n += 1
    return 100.0 * errs / chars, 100.0 * exact / max(1, n), worst_sum


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--new", required=True)
    ap.add_argument("--old", required=True)
    ap.add_argument("--data", required=True)
    args = ap.parse_args()

    table = load_dict()
    so = ort.SessionOptions()
    so.log_severity_level = 3
    new = ort.InferenceSession(args.new, so, providers=["CPUExecutionProvider"])
    old = ort.InferenceSession(args.old, so, providers=["CPUExecutionProvider"])
    ok = check_shapes(old, "old") and check_shapes(new, "new")

    res = {}
    for split in ("val_pixel.txt", "val_normal.txt"):
        o = run(old, args.data, split, table)
        w = run(new, args.data, split, table)
        res[split] = (o, w)
        print(f"{split:16} CER old {o[0]:6.2f} %  new {w[0]:6.2f} %   exact old {o[1]:5.1f} %  new {w[1]:5.1f} %")
        if w[2] > 1e-3:
            print(f"  WRONG: a time step of the new model does not add up to 1 (off by {w[2]:.4f}): not softmax")
            ok = False

    (po, pn), (no, nn) = res["val_pixel.txt"], res["val_normal.txt"]
    if nn[0] > no[0] + 0.5:
        print(f"FAIL: normal fonts got worse by {nn[0] - no[0]:.2f} points (at most 0.5)")
        ok = False
    if pn[0] >= po[0]:
        print("FAIL: pixel fonts did not get better")
        ok = False
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
