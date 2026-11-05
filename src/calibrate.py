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
import gi
import numpy as np
gi.require_version("Gst", "1.0")
from gi.repository import Gst

from make_charuco import make_board

SOI = b"\xff\xd8"


def jpeg_frames(clip, stride=1):
    """Yield (frame_index, jpeg_bytes) for every stride-th frame of a record.py .mkv.

    Frames are demuxed with GStreamer and decoded later with OpenCV, so skipped
    frames are never decoded. Not cv2.VideoCapture: its FFmpeg decoder stops at
    the first frame that doesn't start with SOI, and the camera has produced a
    whole clip like that (each frame preceded by ~10 KB of the previous one).
    """
    if not os.path.isfile(clip):
        raise SystemExit(f"can't open {clip}")
    Gst.init(None)
    pipeline = Gst.parse_launch(
        f"filesrc location={clip} ! matroskademux ! appsink name=sink sync=false")
    sink = pipeline.get_by_name("sink")
    pipeline.set_state(Gst.State.PLAYING)
    try:
        idx = 0
        while True:
            sample = sink.emit("try-pull-sample", 5 * Gst.SECOND)
            if sample is None:
                break
            if idx % stride == 0:
                buf = sample.get_buffer()
                yield idx, buf.extract_dup(0, buf.get_size())
            idx += 1
    finally:
        pipeline.set_state(Gst.State.NULL)


def decode(jpeg, flags=cv2.IMREAD_GRAYSCALE):
    """Decode one frame, skipping anything before SOI; returns (image or None, misframed)."""
    k = jpeg.find(SOI)
    if k < 0:
        return None, True
    return cv2.imdecode(np.frombuffer(jpeg, np.uint8, offset=k), flags), k > 0


def collect_views(clip, board, stride, min_corners, min_sharpness):
    """Return list of dicts (frame, obj, img, sharpness, centre, area), image size,
    and decode stats for the examined frames."""
    detector = cv2.aruco.CharucoDetector(board)
    views, size = [], None
    stats = {"examined": 0, "misframed": 0, "undecodable": 0}
    for idx, jpeg in jpeg_frames(clip, stride):
        stats["examined"] += 1
        gray, misframed = decode(jpeg)
        stats["misframed"] += misframed
        if gray is None:
            stats["undecodable"] += 1
            continue
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
    return views, size, stats


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


ZONES = [["top-left", "top", "top-right"],
         ["left", "centre", "right"],
         ["bottom-left", "bottom", "bottom-right"]]


def coverage_report(views, size, edge_frac, min_points):
    """Corner counts in 3x3 zones whose outer bands are edge_frac of width/height."""
    W, H = size
    pts = np.concatenate([v["img"].reshape(-1, 2) for v in views])

    def band(u):
        return np.where(u < edge_frac, 0, np.where(u > 1 - edge_frac, 2, 1))

    col, row = band(pts[:, 0] / W), band(pts[:, 1] / H)
    counts = {ZONES[r][c]: int(np.sum((row == r) & (col == c)))
              for r in range(3) for c in range(3)}
    return {
        "edge_frac": edge_frac,
        "zone_points": counts,
        "sparse_zones": [z for z, n in counts.items() if n < min_points],
        "extent_pct": {"x": [round(float(100 * pts[:, 0].min() / W), 1),
                             round(float(100 * pts[:, 0].max() / W), 1)],
                       "y": [round(float(100 * pts[:, 1].min() / H), 1),
                             round(float(100 * pts[:, 1].max() / H), 1)]},
    }


def field_angles(K, D, size, max_asym_deg):
    """Viewing angles of the image corners and edge midpoints under the fitted model.

    A probe is "valid" only if its undistorted ray projects back onto the same
    pixel; where the polynomial model is extrapolated past the data it often
    doesn't invert, and those angles are meaningless.
    """
    W, H = size
    cx, cy = K[0, 2], K[1, 2]
    probes = {"left": (0, cy), "right": (W - 1, cy), "top": (cx, 0), "bottom": (cx, H - 1),
              "top-left": (0, 0), "top-right": (W - 1, 0),
              "bottom-left": (0, H - 1), "bottom-right": (W - 1, H - 1)}
    px = np.array(list(probes.values()), np.float64).reshape(-1, 1, 2)
    crit = (cv2.TERM_CRITERIA_COUNT | cv2.TERM_CRITERIA_EPS, 200, 1e-10)
    if hasattr(cv2, "undistortPointsIter"):  # OpenCV 4.x (the Orin)
        norm = cv2.undistortPointsIter(px, K, D, None, None, crit)
    else:  # OpenCV 5.x folded the criteria into undistortPoints
        norm = cv2.undistortPoints(px, K, D, criteria=crit)
    norm = norm.reshape(-1, 2)
    back, _ = cv2.projectPoints(np.c_[norm, np.ones(len(norm))], np.zeros(3), np.zeros(3), K, D)
    err = np.hypot(*(back.reshape(-1, 2) - px.reshape(-1, 2)).T)

    probe_out = {}
    for (name, _), (xn, yn), e in zip(probes.items(), norm, err):
        probe_out[name] = {"h_deg": round(float(np.degrees(np.arctan(xn))), 1),
                           "v_deg": round(float(np.degrees(np.arctan(yn))), 1),
                           "off_axis_deg": round(float(np.degrees(np.arctan(np.hypot(xn, yn)))), 1),
                           "valid": bool(e < 1.0)}
    p = probe_out
    hfov = p["right"]["h_deg"] - p["left"]["h_deg"]
    vfov = p["bottom"]["v_deg"] - p["top"]["v_deg"]
    h_asym = p["right"]["h_deg"] + p["left"]["h_deg"]
    v_asym = p["bottom"]["v_deg"] + p["top"]["v_deg"]
    warnings = [f"{n}: model doesn't invert here (extrapolated)" for n, v in p.items() if not v["valid"]]
    if abs(h_asym) > max_asym_deg:
        warnings.append(f"left/right asymmetric by {h_asym:+.1f} deg")
    if abs(v_asym) > max_asym_deg:
        warnings.append(f"top/bottom asymmetric by {v_asym:+.1f} deg")
    return {"hfov_deg": round(float(hfov), 1), "vfov_deg": round(float(vfov), 1),
            "h_asym_deg": round(float(h_asym), 1), "v_asym_deg": round(float(v_asym), 1),
            "probes": probe_out, "warnings": warnings}


def print_reports(cov, ang):
    print(f"coverage (outer {100 * cov['edge_frac']:.0f}% bands), corner points per zone:")
    for row in ZONES:
        print("   " + "".join(f"{z:>13}:{cov['zone_points'][z]:5d}" for z in row))
    e = cov["extent_pct"]
    print(f"   points span x {e['x'][0]}-{e['x'][1]}%, y {e['y'][0]}-{e['y'][1]}% of the image")
    if cov["sparse_zones"]:
        print("   SPARSE:", ", ".join(cov["sparse_zones"]), "-- record more board poses there")
    print(f"field of view: H {ang['hfov_deg']} deg (asym {ang['h_asym_deg']:+}), "
          f"V {ang['vfov_deg']} deg (asym {ang['v_asym_deg']:+})")
    for name, p in ang["probes"].items():
        flag = "" if p["valid"] else "  INVALID"
        print(f"   {name:>12}: h {p['h_deg']:+6.1f}  v {p['v_deg']:+6.1f}  "
              f"off-axis {p['off_axis_deg']:5.1f} deg{flag}")
    for w in ang["warnings"]:
        print("   WARNING:", w)


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
    ap.add_argument("--edge-frac", type=float, default=0.15,
                    help="width of the edge/corner bands for the coverage report")
    ap.add_argument("--min-zone-points", type=int, default=30,
                    help="corners needed in a zone before it isn't flagged sparse")
    ap.add_argument("--max-asym-deg", type=float, default=5.0,
                    help="flag left/right or top/bottom field-angle asymmetry above this")
    args = ap.parse_args()

    with open(args.board) as f:
        board_params = json.load(f)
    board = make_board(board_params)

    views, size, decode_stats = collect_views(args.clip, board, args.stride,
                                              args.min_corners, args.min_sharpness)
    print(f"examined {decode_stats['examined']} frames: {decode_stats['misframed']} misframed "
          f"(data before SOI, decoded anyway), {decode_stats['undecodable']} undecodable")
    if decode_stats["examined"] == 0 or decode_stats["undecodable"] == decode_stats["examined"]:
        raise SystemExit(f"no decodable frames in {args.clip}")
    if not views:
        raise SystemExit("no usable board views found -- check --board matches the printed board, "
                         "and focus/exposure/--min-sharpness")
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
    cov = coverage_report(picked, size, args.edge_frac, args.min_zone_points)
    ang = field_angles(K, D, size, args.max_asym_deg)

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
        "decode": decode_stats,
        "coverage": cov,
        "field_angles": ang,
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

    sample_idx = min(v["frame"] for v in train)  # earliest, so the scan stays short
    for idx, jpeg in jpeg_frames(args.clip):
        if idx == sample_idx:
            frame, _ = decode(jpeg, cv2.IMREAD_COLOR)
            if frame is not None:
                cv2.imwrite(os.path.join(args.out, f"undistorted_{args.name}.png"),
                            cv2.undistort(frame, K, D))
            break

    worst = sorted(result["per_view_rms_px"].items(), key=lambda kv: -kv[1])[:5]
    print(f"RMS {rms_fit:.3f} px, validation RMS "
          f"{'n/a' if val_rms is None else f'{val_rms:.3f}'} px")
    print("worst views (frame: px):", ", ".join(f"{k}: {e:.2f}" for k, e in worst))
    print_reports(cov, ang)
    print(f"wrote results to {args.out}")


if __name__ == "__main__":
    main()
