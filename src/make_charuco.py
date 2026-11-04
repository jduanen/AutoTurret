#!/usr/bin/env python3
"""
Generate the Phase 1 ChArUco calibration board (README: 7x5 squares, 50mm
squares, 35mm markers, DICT_5X5_100) as a print-ready PNG plus a JSON of the
board parameters that calibrate.py reads.
"""

import argparse
import json
import os

import cv2

SQUARES_X, SQUARES_Y = 7, 5
SQUARE_MM, MARKER_MM = 50.0, 35.0
DICTIONARY = "DICT_5X5_100"


def make_board(params):
    """Build the cv2 CharucoBoard described by a board-params dict (lengths in metres)."""
    return cv2.aruco.CharucoBoard(
        (params["squares_x"], params["squares_y"]),
        params["square_m"], params["marker_m"],
        cv2.aruco.getPredefinedDictionary(getattr(cv2.aruco, params["dictionary"])))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default=os.path.expanduser("~/at/board"))
    ap.add_argument("--dpi", type=int, default=300)
    ap.add_argument("--margin-mm", type=float, default=10.0)
    args = ap.parse_args()

    params = {"squares_x": SQUARES_X, "squares_y": SQUARES_Y,
              "square_m": SQUARE_MM / 1000, "marker_m": MARKER_MM / 1000,
              "dictionary": DICTIONARY}
    board = make_board(params)

    px_per_mm = args.dpi / 25.4
    margin = round(args.margin_mm * px_per_mm)
    w = round(SQUARES_X * SQUARE_MM * px_per_mm) + 2 * margin
    h = round(SQUARES_Y * SQUARE_MM * px_per_mm) + 2 * margin
    img = board.generateImage((w, h), marginSize=margin, borderBits=1)

    # self-check: the detector should find every inner corner on the clean image
    corners, ids, _, _ = cv2.aruco.CharucoDetector(board).detectBoard(img)
    n_expected = (SQUARES_X - 1) * (SQUARES_Y - 1)
    n_found = 0 if ids is None else len(ids)

    os.makedirs(args.out, exist_ok=True)
    png = os.path.join(args.out, "charuco_7x5_50mm.png")
    cv2.imwrite(png, img)
    with open(os.path.join(args.out, "board.json"), "w") as f:
        json.dump(params, f, indent=2)

    print(f"wrote {png}: {w}x{h} px at {args.dpi} dpi "
          f"({w / px_per_mm:.0f}x{h / px_per_mm:.0f} mm incl. {args.margin_mm:.0f} mm margin)")
    print(f"self-check: {n_found}/{n_expected} corners detected")
    print("Print at 100% (no fit-to-page) on matte paper, mount flat on rigid backing,\n"
          "then measure a square; edit square_m/marker_m in board.json if it is off.")


if __name__ == "__main__":
    main()
