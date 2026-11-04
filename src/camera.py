"""
See3CAM_24CUG helpers: device lookup, manual controls, GStreamer source string.

Exposure/gain/WB values are raw V4L2 control values (UVC exposure_time_absolute
is nominally in 100 usec units -- verify against the camera).
"""

import glob
import os
import subprocess


def find_device(name="See3CAM_24CUG"):
    """Return the first /dev/videoN whose V4L2 name matches and that can capture."""
    for path in sorted(glob.glob("/sys/class/video4linux/video*")):
        with open(os.path.join(path, "name")) as f:
            if name not in f.read():
                continue
        dev = "/dev/" + os.path.basename(path)
        out = subprocess.run(["v4l2-ctl", "-d", dev, "--list-formats"],
                             capture_output=True, text=True).stdout
        if "MJPG" in out:
            return dev
    raise RuntimeError(f"no capture device named {name!r} found")


def get_controls(dev):
    """Return {control_name: int} for the device's current control values."""
    out = subprocess.run(["v4l2-ctl", "-d", dev, "--list-ctrls"],
                         capture_output=True, text=True, check=True).stdout
    ctrls = {}
    for line in out.splitlines():
        if "value=" not in line:
            continue
        ctrl_name = line.split()[0]
        value = line.split("value=")[1].split()[0]
        ctrls[ctrl_name] = int(value)
    return ctrls


def set_controls(dev, exposure=None, gain=None, wb_temp=None):
    """Put the camera in manual exposure/gain/WB (where given); return readback."""
    sets = []
    if exposure is not None:
        sets += ["auto_exposure=1", f"exposure_time_absolute={exposure}"]
    if gain is not None:
        sets.append(f"gain={gain}")
    if wb_temp is not None:
        sets += ["white_balance_automatic=0", f"white_balance_temperature={wb_temp}"]
    # auto-mode switches must land before the values they unlock, so set one at a time
    for s in sets:
        subprocess.run(["v4l2-ctl", "-d", dev, "-c", s], check=True)
    return get_controls(dev)


def source_pipeline(dev, width, height, fps):
    """GStreamer source fragment producing MJPEG buffers stamped at capture.

    Buffers from the element named "src" carry the capture timestamp (pts) and
    the driver's frame sequence number (offset). Don't add jpegparse: it
    replaces the real timestamps with ones interpolated from the framerate.
    """
    return (f"v4l2src name=src device={dev} do-timestamp=true ! "
            f"image/jpeg,width={width},height={height},framerate={fps}/1")
