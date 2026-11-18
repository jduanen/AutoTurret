#!/usr/bin/env python3
"""
Ultralytics'-YOLO-based object detector
"""

import argparse
import json
import os
import time
from collections import Counter
import logging

os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;tcp"

import pdb
import torch
from ultralytics import YOLO
from ultralytics.utils import LOGGER

YOLO_MODEL_NAME = "yolo26x.pt"


class IgnoreStreamWait(logging.Filter):
    def filter(self, record):
        return "Waiting for stream" not in record.getMessage()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default=os.getenv("CAMERA_HOST"),
                    help="Camera IP Address (default: CAMERA_HOST env var)")
    ap.add_argument("--user", default=os.getenv("CAMERA_USER"),
                    help="Camera user name (default: CAMERA_USER env var)")
    ap.add_argument("--password", default=os.getenv("CAMERA_PASSWORD"),
                    help="Camera password (default: CAMERA_PASSWORD env var)")
    ap.add_argument("--stream", type=int, choices=[1,2], default=2,
                    help="Select stream1 or stream2 (default: %(default)s)")
    ap.add_argument("--name", default="rtspCam",
                    help="Name of this camera (default: %(default)s)")
    ap.add_argument("--loglevel",
                    choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITCAL"],
                    default="INFO",
                    help="Set the logging level (default: %(default)s)")
    args = ap.parse_args()
    if not (args.host and args.user and args.password):
        ap.error("must give host, user, and password, as cmd line args or env vars")

    logging.basicConfig(
        level=args.loglevel,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    rtsp_url = f"rtsp://{args.user}:{args.password}@{args.host}:554/stream{args.stream}"
    logging.debug("Camera URL: %s", rtsp_url)

    if not torch.cuda.is_available():
        raise RuntimeError(
            "CUDA is unavailable in this Python environment. "
            "Check the NVIDIA driver and PyTorch installation."
        )

    logging.debug("GPU: %s", torch.cuda.get_device_name(0))

    model = YOLO("yolo26x.pt")
    model.to("cuda:0")
    logging.debug("Model device after move: %s", model.device)

    LOGGER.addFilter(IgnoreStreamWait())    # suppress library spam

    results = model.predict(
        source=rtsp_url,
        stream=True,
        stream_buffer=False,
        device=0,  # first CUDA GPU
        conf=0.5,
        imgsz=640,  # required by YOLO
        verbose=False,
    )

    previous_counts = None
    last_emit = 0.0

    frames_processed = 0
    last_status = time.monotonic()

    for result in results:
##        pdb.set_trace()
        counts = Counter(
            result.names[int(class_id)]
            for class_id in result.boxes.cls.cpu().tolist()
        )
        logging.debug("%s", dict(counts))

        now = time.monotonic()
        if counts != previous_counts or now - last_emit >= 5:
            description = (
                ", ".join(
                f"{count} {name}"
                for name, count in sorted(counts.items())
            )
        )

        if dict(counts):
            print(
                json.dumps({
                    "timestamp": time.time(),
                    "camera": args.name,
                    "objects": dict(counts),
                    "text": description,
                }),
                flush=True,
            )

        previous_counts = counts.copy()
        last_emit = now 
        frames_processed += 1
        now = time.monotonic()

        if now - last_status >= 5:
            elapsed = now - last_status
            logging.info("Processed %d frames in %.1f sec (%.1f FPS)",
                         frames_processed, elapsed, (frames_processed / elapsed))
            frames_processed = 0
            last_status = now

if __name__ == "__main__":
    main()
