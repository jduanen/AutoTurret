#!/usr/bin/env python3
"""
Fit camera intrinsics (K, D) from a ChArUco calibration clip made with record.py.

Samples frames, rejects blurred/poorly-detected views, picks views spread over
the image (position x board-size buckets), holds some out for validation, runs
calibrateCameraExtended, and writes calib JSON plus diagnostic PNGs.
"""

import argparse
import json
import os

import cv2
import numpy as np

from make_charuco import make_board


def collect_views(clip, board, stride, min_corners, min_sharpness):
    """Return list of dicts (frame, obj, img, sharpness, centre, area) and image size."""
    detector = cv2.aruco.CharucoDetector(board)
    cap = cv2.VideoCapture(clip, cv2.CAP_FFMPEG)
    views, size, idx = [], None, -1
    while True:
        ok, frame = cap.read()
        if not ok:
            break
        idx += 1
        if idx % stride:
            continue
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        size = gray.shape[::-1]
        corners, ids, _, _ = detector.detectBoard(gray)
        if ids is None or len(ids) < min_corners:
            continue
        x, y, w, h = cv2.boundingRect(corners.astype(np.float32))
        sharp = cv2.Laplacian(gray[y:y + h, x:x + w], cv2.CV_64F).var()
        if sharp < min_sharpness:
            continue
        obj, img = board.matchImagePoints(corners, ids)
        views.append({"frame": idx, "obj": obj, "img": img, "sharpness": sharp,
                      "centre": (x + w / 2, y + h / 2), "area": w * h})
    cap.release()
    return views, size


def select_views(views, size, max_views):
    """Round-robin the sharpest view from each (3x3 position, 3 size) bucket."""
    W, H = size
    buckets = {}
    for v in views:
        cx, cy = v["centre"]
        frac = v["area"] / (W * H)
        key = (min(int(3 * cx / W), 2), min(int(3 * cy / H), 2),
               0 if frac < 0.1 else 1 if frac < 0.3 else 2)
        buckets.setdefault(key, []).append(v)
    for b in buckets.values():
        b.sort(key=lambda v: -v["sharpness"])
    picked = []
    while len(picked) < max_views and any(buckets.values()):
        for b in buckets.values():
            if b and len(picked) < max_views:
                picked.append(b.pop(0))
    return picked, len(buckets)


def reproj_errors(view, K, D, rvec=None, tvec=None):
    """Per-point residual vectors (N,2); solves pose first if none given."""
    if rvec is None:
        _, rvec, tvec = cv2.solvePnP(view["obj"], view["img"], K, D)
    proj, _ = cv2.projectPoints(view["obj"], rvec, tvec, K, D)
    return view["img"].reshape(-1, 2) - proj.reshape(-1, 2)


def rms(residuals):
    r = np.concatenate(residuals)
    return float(np.sqrt((r ** 2).sum(axis=1).mean()))


def draw_residuals(size, train, K, D, rvecs, tvecs, scale):
    W, H = size
    out = np.full((H, W, 3), 255, np.uint8)
    for v, rv, tv in zip(train, rvecs, tvecs):
        res = reproj_errors(v, K, D, rv, tv)
        for (px, py), (dx, dy) in zip(v["img"].reshape(-1, 2), res):
            p0 = (int(px), int(py))
            p1 = (int(px + scale * dx), int(py + scale * dy))
            cv2.arrowedLine(out, p0, p1, (0, 0, 200), 2, tipLength=0.3)
    cv2.putText(out, f"residuals x{scale}", (20, 50), cv2.FONT_HERSHEY_SIMPLEX, 1.5,
                (0, 0, 0), 3)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("clip")
    ap.add_argument("--board", default=os.path.expanduser("~/at/board/board.json"))
    ap.add_argument("--out", default=os.path.expanduser("~/at/calib"))
    ap.add_argument("--name", default="24cug")
    ap.add_argument("--stride", type=int, default=10, help="examine every Nth frame")
    ap.add_argument("--min-corners", type=int, default=8)
    ap.add_argument("--min-sharpness", type=float, default=100.0,
                    help="Laplacian variance over the board region")
    ap.add_argument("--max-views", type=int, default=50)
    ap.add_argument("--holdout-every", type=int, default=7,
                    help="every Nth selected view goes to validation")
    ap.add_argument("--residual-scale", type=float, default=50.0)
    args = ap.parse_args()

    with open(args.board) as f:
        board_params = json.load(f)
    board = make_board(board_params)

    views, size = collect_views(args.clip, board, args.stride,
                                args.min_corners, args.min_sharpness)
    picked, n_buckets = select_views(views, size, args.max_views)
    val = picked[args.holdout_every - 1::args.holdout_every]
    val_ids = {id(v) for v in val}  # identity: views hold numpy arrays
    train = [v for v in picked if id(v) not in val_ids]
    print(f"{len(views)} usable views, {len(picked)} selected over {n_buckets} buckets "
          f"({len(train)} train / {len(val)} validation)")
    if len(train) < 10:
        raise SystemExit("too few usable views -- record more board poses")

    rms_fit, K, D, rvecs, tvecs, std_int, _, per_view = cv2.calibrateCameraExtended(
        [v["obj"] for v in train], [v["img"] for v in train], size, None, None)
    val_rms = rms([reproj_errors(v, K, D) for v in val]) if val else None

    os.makedirs(args.out, exist_ok=True)
    result = {
        "camera": args.name,
        "clip": os.path.basename(args.clip),
        "image_size": list(size),
        "K": K.tolist(),
        "D": D.ravel().tolist(),
        "std_intrinsics": std_int.ravel().tolist(),
        "rms_px": rms_fit,
        "validation_rms_px": val_rms,
        "per_view_rms_px": {v["frame"]: float(e) for v, e in zip(train, per_view.ravel())},
        "train_frames": [v["frame"] for v in train],
        "validation_frames": [v["frame"] for v in val],
        "board": board_params,
    }
    with open(os.path.join(args.out, f"calib_{args.name}.json"), "w") as f:
        json.dump(result, f, indent=2)

    cv2.imwrite(os.path.join(args.out, f"residuals_{args.name}.png"),
                draw_residuals(size, train, K, D, rvecs, tvecs, args.residual_scale))
    coverage = np.full((size[1], size[0], 3), 255, np.uint8)
    for v in picked:
        color = (200, 0, 0) if id(v) in val_ids else (0, 150, 0)
        for px, py in v["img"].reshape(-1, 2):
            cv2.circle(coverage, (int(px), int(py)), 6, color, -1)
    cv2.imwrite(os.path.join(args.out, f"coverage_{args.name}.png"), coverage)

    cap = cv2.VideoCapture(args.clip, cv2.CAP_FFMPEG)
    cap.set(cv2.CAP_PROP_POS_FRAMES, train[0]["frame"])
    ok, frame = cap.read()
    if ok:
        cv2.imwrite(os.path.join(args.out, f"undistorted_{args.name}.png"),
                    cv2.undistort(frame, K, D))

    worst = sorted(result["per_view_rms_px"].items(), key=lambda kv: -kv[1])[:5]
    print(f"RMS {rms_fit:.3f} px, validation RMS "
          f"{'n/a' if val_rms is None else f'{val_rms:.3f}'} px")
    print("worst views (frame: px):", ", ".join(f"{k}: {e:.2f}" for k, e in worst))
    print(f"wrote results to {args.out}")


if __name__ == "__main__":
    main()
