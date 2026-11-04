#!/usr/bin/env python3
"""
Phase 1 motion-candidate detector: MOG2 background subtraction, connected
components, and greedy nearest-neighbour track association.

Runs on a recorded .mkv (from record.py) or live from the camera. Writes an
overlay video, a per-detection CSV, and a timing summary.
"""

import argparse
import csv
import json
import os
import time

import cv2
import gi
import numpy as np
gi.require_version("Gst", "1.0")
from gi.repository import Gst

import camera

# nvjpegdec can't decode this camera's MJPEG on JetPack 7.2, so decode on the CPU
DECODE = "jpegdec ! videoconvert ! video/x-raw,format=GRAY8 ! appsink name=sink"


class Tracker:
    """Greedy nearest-neighbour association of centroids into tracks."""

    def __init__(self, max_jump, max_missed):
        self.max_jump, self.max_missed = max_jump, max_missed
        self.tracks = {}  # id -> {"pos", "last", "hits", "trail"}
        self.next_id = 0

    def update(self, frame_no, centroids):
        """Return the track id assigned to each centroid."""
        assigned = [None] * len(centroids)
        tids = list(self.tracks)
        if tids and centroids:
            c = np.asarray(centroids)
            p = np.array([self.tracks[tid]["pos"] for tid in tids])
            d = np.hypot(c[:, None, 0] - p[None, :, 0], c[:, None, 1] - p[None, :, 1])
            ci, ti = np.nonzero(d <= self.max_jump)  # only gated pairs
            used = set()
            for k in np.argsort(d[ci, ti], kind="stable"):
                a, b = ci[k], ti[k]
                if assigned[a] is None and b not in used:
                    assigned[a] = tids[b]
                    used.add(b)
        for ci, c in enumerate(centroids):
            if assigned[ci] is None:
                assigned[ci] = self.next_id
                self.tracks[self.next_id] = {"hits": 0, "trail": []}
                self.next_id += 1
            t = self.tracks[assigned[ci]]
            t["pos"], t["last"] = c, frame_no
            t["hits"] += 1
            t["trail"] = (t["trail"] + [(int(c[0]), int(c[1]))])[-30:]
        for tid in [tid for tid, t in self.tracks.items()
                    if frame_no - t["last"] > self.max_missed]:
            del self.tracks[tid]
        return assigned


def build_pipeline(args):
    if args.live:
        dev = args.device or camera.find_device()
        camera.set_controls(dev, args.exposure, args.gain, args.wb)
        w, h = (int(v) for v in args.size.split("x"))
        # leaky queue decouples capture from decode so the camera never stalls
        src = camera.source_pipeline(dev, w, h, args.fps) + " ! queue leaky=downstream max-size-buffers=2"
        sink_opts = " drop=true max-buffers=1 sync=false"  # never process stale frames
    else:
        src = f"filesrc location={args.source} ! matroskademux"
        sink_opts = " sync=false"
    return Gst.parse_launch(f"{src} ! {DECODE}{sink_opts}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source", nargs="?", help=".mkv clip (omit with --live)")
    ap.add_argument("--live", action="store_true", help="read from the camera instead")
    ap.add_argument("--seconds", type=float, default=10.0, help="live run length")
    ap.add_argument("--device")
    ap.add_argument("--size", default="1920x1080")
    ap.add_argument("--fps", type=int, default=60)
    ap.add_argument("--exposure", type=int)
    ap.add_argument("--gain", type=int)
    ap.add_argument("--wb", type=int)
    ap.add_argument("--out", default=os.path.expanduser("~/at/md"))
    ap.add_argument("--history", type=int, default=500)
    ap.add_argument("--var-threshold", type=float, default=16.0)
    ap.add_argument("--learning-rate", type=float, default=-1.0, help="-1 = automatic")
    ap.add_argument("--min-area", type=int, default=20, help="pixels")
    ap.add_argument("--max-area", type=int, default=20000, help="pixels")
    ap.add_argument("--max-aspect", type=float, default=6.0)
    ap.add_argument("--max-candidates", type=int, default=50,
                    help="keep only the largest N per frame (bounds cost in noisy scenes)")
    ap.add_argument("--max-jump", type=float, default=150.0, help="pixels per frame")
    ap.add_argument("--max-missed", type=int, default=5, help="frames before a track is dropped")
    ap.add_argument("--min-track-len", type=int, default=3, help="hits before a track is drawn")
    ap.add_argument("--no-video", action="store_true", help="skip the overlay video")
    args = ap.parse_args()
    if not args.live and not args.source:
        ap.error("give a clip or --live")

    Gst.init(None)
    pipeline = build_pipeline(args)
    sink = pipeline.get_by_name("sink")
    pipeline.set_state(Gst.State.PLAYING)

    mog = cv2.createBackgroundSubtractorMOG2(args.history, args.var_threshold,
                                             detectShadows=False)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    tracker = Tracker(args.max_jump, args.max_missed)

    os.makedirs(args.out, exist_ok=True)
    stem = "live" if args.live else os.path.splitext(os.path.basename(args.source))[0]
    csv_file = open(os.path.join(args.out, f"{stem}_candidates.csv"), "w", newline="")
    rows = csv.writer(csv_file)
    rows.writerow(["frame", "timestamp_ms", "track_id", "track_hits",
                   "x", "y", "w", "h", "area"])
    writer = None

    proc_ms, age_ms, frames_capped = [], [], 0
    frame_no, t_end = 0, time.monotonic() + args.seconds
    while not (args.live and time.monotonic() > t_end):
        sample = sink.emit("try-pull-sample", 5 * Gst.SECOND)
        if sample is None:
            break  # EOS (file) or camera stalled
        buf = sample.get_buffer()
        caps = sample.get_caps().get_structure(0)
        w, h = caps.get_value("width"), caps.get_value("height")
        ok, info = buf.map(Gst.MapFlags.READ)
        stride = info.size // h  # GRAY8 rows may be padded
        gray = np.ndarray((h, stride), np.uint8, info.data)[:, :w].copy()
        buf.unmap(info)
        t0 = time.perf_counter()

        fg = mog.apply(gray, learningRate=args.learning_rate)
        fg = cv2.morphologyEx(fg, cv2.MORPH_OPEN, kernel)
        fg = cv2.morphologyEx(fg, cv2.MORPH_CLOSE, kernel)
        n, _, stats, cents = cv2.connectedComponentsWithStats(fg, connectivity=8)
        st, ce = stats[1:], cents[1:]  # drop background label
        area = st[:, 4]
        aspect = np.maximum(st[:, 2], st[:, 3]) / np.maximum(1, np.minimum(st[:, 2], st[:, 3]))
        keep = np.nonzero((area >= args.min_area) & (area <= args.max_area)
                          & (aspect <= args.max_aspect))[0]
        if len(keep) > args.max_candidates:
            keep = keep[np.argsort(-area[keep], kind="stable")[:args.max_candidates]]
            frames_capped += 1
        boxes = [tuple(int(v) for v in st[i]) for i in keep]
        centroids = [tuple(ce[i]) for i in keep]
        ids = tracker.update(frame_no, centroids)

        proc_ms.append((time.perf_counter() - t0) * 1000)
        if args.live:
            now = pipeline.get_clock().get_time() - pipeline.get_base_time()
            age_ms.append((now - buf.pts) / 1e6)

        ts_ms = buf.pts / 1e6
        for (x, y, bw, bh, area), tid in zip(boxes, ids):
            rows.writerow([frame_no, f"{ts_ms:.3f}", tid, tracker.tracks[tid]["hits"],
                           x, y, bw, bh, area])

        if not args.no_video:
            if writer is None:
                writer = cv2.VideoWriter(os.path.join(args.out, f"{stem}_overlay.avi"),
                                         cv2.VideoWriter_fourcc(*"MJPG"), args.fps, (w, h))
            vis = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
            for (x, y, bw, bh, _), tid in zip(boxes, ids):
                t = tracker.tracks[tid]
                if t["hits"] < args.min_track_len:
                    cv2.rectangle(vis, (x, y), (x + bw, y + bh), (0, 160, 255), 1)
                    continue
                cv2.rectangle(vis, (x, y), (x + bw, y + bh), (0, 255, 0), 2)
                cv2.putText(vis, str(tid), (x, y - 4), cv2.FONT_HERSHEY_SIMPLEX, 0.6,
                            (0, 255, 0), 2)
                cv2.polylines(vis, [np.array(t["trail"], np.int32)], False, (255, 0, 255), 1)
            cv2.putText(vis, f"#{frame_no}  {ts_ms:.1f} ms", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)
            writer.write(vis)
        frame_no += 1

    pipeline.set_state(Gst.State.NULL)
    csv_file.close()
    if writer is not None:
        writer.release()

    def stats(v):
        return None if not v else {"mean": float(np.mean(v)), "p95": float(np.percentile(v, 95)),
                                    "max": float(np.max(v))}

    summary = {"source": "live" if args.live else args.source, "frames": frame_no,
               "tracks_created": tracker.next_id, "frames_capped": frames_capped,
               "processing_ms": stats(proc_ms),
               "frame_age_ms": stats(age_ms), "params": vars(args)}
    with open(os.path.join(args.out, f"{stem}_summary.json"), "w") as f:
        json.dump(summary, f, indent=2)
    print(json.dumps({k: summary[k] for k in
                      ("frames", "tracks_created", "frames_capped", "processing_ms",
                       "frame_age_ms")}, indent=2))


if __name__ == "__main__":
    main()
