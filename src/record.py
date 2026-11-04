#!/usr/bin/env python3
"""
Record MJPEG passthrough clips from the See3CAM_24CUG (no re-encoding).

Writes <label>_<timestamp>.mkv and a matching .json with capture settings,
control readback, per-frame timing stats, and dropped frames (from gaps in the
driver's frame sequence numbers).
"""

import argparse
import json
import os
import signal
import time

import gi
gi.require_version("Gst", "1.0")
from gi.repository import GLib, Gst

import camera


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--fps", type=int, default=60, choices=[30, 60, 114, 120])
    ap.add_argument("--size", default="1920x1080")
    ap.add_argument("--exposure", type=int, help="raw exposure_time_absolute (manual if given)")
    ap.add_argument("--gain", type=int)
    ap.add_argument("--wb", type=int, help="white balance temperature (manual if given)")
    ap.add_argument("--device", help="default: find See3CAM_24CUG")
    ap.add_argument("--out", default=os.path.expanduser("~/at/clips"))
    ap.add_argument("--label", default="clip")
    args = ap.parse_args()

    width, height = (int(v) for v in args.size.split("x"))
    dev = args.device or camera.find_device()
    ctrls = camera.set_controls(dev, args.exposure, args.gain, args.wb)

    os.makedirs(args.out, exist_ok=True)
    stem = os.path.join(args.out, f"{args.label}_{time.strftime('%Y%m%d_%H%M%S')}")

    Gst.init(None)
    pipeline = Gst.parse_launch(
        camera.source_pipeline(dev, width, height, args.fps)
        + f" ! matroskamux ! filesink location={stem}.mkv")

    pts, seq = [], []

    def on_buffer(pad, info):
        buf = info.get_buffer()
        pts.append(buf.pts)
        seq.append(buf.offset)  # driver frame sequence number
        return Gst.PadProbeReturn.OK

    pipeline.get_by_name("src").get_static_pad("src").add_probe(
        Gst.PadProbeType.BUFFER, on_buffer)

    loop = GLib.MainLoop()

    def on_message(bus, msg):
        if msg.type == Gst.MessageType.EOS:
            loop.quit()
        elif msg.type == Gst.MessageType.ERROR:
            err, dbg = msg.parse_error()
            print(f"ERROR: {err.message}\n{dbg}")
            loop.quit()

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect("message", on_message)

    def stop(*_):
        pipeline.send_event(Gst.Event.new_eos())  # finalize the mkv cleanly
        return False

    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGINT, stop)
    GLib.timeout_add(int(args.seconds * 1000), stop)

    wall_start = time.time()
    pipeline.set_state(Gst.State.PLAYING)
    loop.run()
    pipeline.set_state(Gst.State.NULL)

    intervals = [(b - a) / 1e6 for a, b in zip(pts, pts[1:])]
    dropped = sum(b - a - 1 for a, b in zip(seq, seq[1:]))
    meta = {
        "file": os.path.basename(stem + ".mkv"),
        "device": dev,
        "start_wallclock": wall_start,
        "width": width, "height": height, "fps": args.fps, "format": "MJPG",
        "controls": ctrls,
        "frames": len(pts),
        "interval_ms_mean": sum(intervals) / len(intervals) if intervals else None,
        "interval_ms_max": max(intervals) if intervals else None,
        "dropped": dropped,
    }
    with open(stem + ".json", "w") as f:
        json.dump(meta, f, indent=2)
    print(json.dumps({k: meta[k] for k in
                      ("file", "frames", "interval_ms_mean", "interval_ms_max", "dropped")},
                     indent=2))


if __name__ == "__main__":
    main()
