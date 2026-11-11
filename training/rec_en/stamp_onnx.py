r"""Writes the Apache-2.0 "modified" notice into an exported rec_en.onnx (task L6).

    python -I stamp_onnx.py <model.onnx> [--commit <git commit of training/rec_en>]

The recogniser is PaddleOCR's en_PP-OCRv3_rec (Apache-2.0), fine-tuned by the Lexia project. Apache-2.0 section 4(b)
asks that a modified file carries a prominent notice that it was changed; an ONNX file has no comment, so the notice
goes into the model's doc_string and metadata_props, where any ONNX viewer shows it. Only metadata changes: the graph,
the weights, the input and the output stay the same, so Lexia (src/ocr.cpp) reads the model as before.

The file is rewritten in place. Running it twice gives the same file (the old notice is replaced, not appended).
"""

import argparse
import sys

import onnx

NOTICE = ("Modified by the Lexia project (https://github.com/Xencaye/Lexia): PaddleOCR's en_PP-OCRv3_rec "
          "(Apache-2.0, Copyright (c) PaddlePaddle Authors) fine-tuned on synthetic lines in pixel game fonts and "
          "normal fonts; the scripts and the font list are in training/rec_en of that repository.")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model")
    ap.add_argument("--commit", default="", help="the git commit the training scripts were at")
    args = ap.parse_args()

    model = onnx.load(args.model)
    model.doc_string = NOTICE
    props = {"license": "Apache-2.0", "modified_by": "Lexia project", "base_model": "PaddleOCR en_PP-OCRv3_rec",
             "notice": NOTICE}
    if args.commit:
        props["training_commit"] = args.commit
    # Replace Lexia's own keys, keep any other metadata the exporter wrote.
    keep = [p for p in model.metadata_props if p.key not in props]
    del model.metadata_props[:]
    model.metadata_props.extend(keep)
    for k, v in props.items():
        model.metadata_props.add(key=k, value=v)
    onnx.checker.check_model(model)
    onnx.save(model, args.model)
    print(f"stamped {args.model}")


if __name__ == "__main__":
    sys.exit(main())
