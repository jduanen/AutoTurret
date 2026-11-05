# AutoTurret Camera Calibration

Calibration measures a camera's intrinsics:
- the camera matrix **K**: focal lengths fx, fy and principal point cx, cy
- the lens distortion coefficients **D**: k1, k2, p1, p2, k3

With them, any pixel can be turned into a viewing ray. That is how a detection in the wide camera becomes a pan/tilt pointing direction. See README.md, *Cameras* and *Phase 1*.

This document covers the fixed wide camera (See3CAM_24CUG on the Orin Nano Super) using the tools in `src/`. The camera's orientation relative to the turret (extrinsics) is a separate, later step.

**Recalibrate whenever** the focus, lens, resolution/crop mode, or the camera's physical lens mounting changes. A calibration is only valid for the exact mode it was recorded in (currently 1920x1080 MJPEG). Frame rate doesn't matter.

## Tools

All tools run on the Orin from `~/Code/AutoTurret`. Outputs go under `~/at/` by default.

| Step | Script | Helper in `bin/` |
|---|---|---|
| Make the board | `src/make_charuco.py` | |
| Check exposure/focus | `src/record.py` (short clip) | `makeTestClip.sh`, `extractFrame.sh` |
| Record calibration clip | `src/record.py` | `makeCalibrationClip.sh` |
| Fit and evaluate | `src/calibrate.py` | `doCalibration.sh` |
| Copy results back | `rsync` | `syncOrin.sh` |

## 1. Make and print the board

The board is a ChArUco board: 7x5 squares, dictionary `DICT_5X5_100`. Two sizes are supported.

| Board | Command | Pattern size | Paper |
|---|---|---|---|
| README spec | `python3 src/make_charuco.py` | 350x250 mm (50 mm squares, 35 mm markers) | 11x17 / A3 (print shop) |
| Letter | `python3 src/make_charuco.py --square-mm 35 --marker-mm 24.5 --out ~/at/board_letter` | 245x175 mm (35 mm squares, 24.5 mm markers) | US Letter landscape |

- Each run writes a 300 dpi PNG and `board.json`, which `calibrate.py` reads. It also checks that all 24 inner corners are detected on the generated image.
- **Generate the board on the Orin**, so the board is drawn by the same OpenCV version (4.8) that detects it.
- PNGs carry no DPI metadata. To print at exact scale, make a PDF on a Letter page (the PNG is in `~/at/board_letter/`; `at/board_letter/` here after syncing):

  ```bash
  magick charuco_7x5_35mm.png -background white -gravity center -extent 3300x2550 -units PixelsPerInch -density 300 charuco_7x5_35mm_letter.pdf
  ```

- Print with scaling off:

  ```bash
  lp -d EPSON_ET_3850_Series -o PageSize=Letter -o print-scaling=none -o cupsPrintQuality=High -o ColorModel=Gray charuco_7x5_35mm_letter.pdf
  ```

  - Use plain or matte paper, not glossy, which glares.
  - Use the non-borderless page size; borderless printing enlarges the image.
  - The landscape page is rotated onto portrait paper without scaling.
- **Mount the print flat on something rigid.** Aluminium composite, glass or acrylic stay flatter than foam board. Spray adhesive is better than tape.
- **Measure it.** Measure across all 7 squares with a steel rule: 245.0 mm for the Letter board, 350.0 mm for the README board. Check one marker too. If the print is off, put the measured sizes, in metres, into `square_m` and `marker_m` in that board's `board.json`. Getting `board.json` right matters more than a perfect print.

The Letter board is small for a 102 deg lens. At 3 m it would be only ~60 px wide, so it has to be held at about 0.5-1.2 m (see step 2).

## 2. Set up the camera

- **Mount the camera rigidly. Set the focus for the working distance** (the feeder, 3-5 m) and lock it. Don't touch the focus afterwards.
- With the 2.8 mm lens, focus set for 3-5 m should still give acceptable sharpness down to roughly 0.5-0.6 m. That is an estimate, so check it on a test frame.
- **Lighting:** bright, even and indirect, such as a shaded window or an overcast sky. Avoid glare on the board.
- Run the Orin in MAXN SUPER (`bin/setPowerMode.sh`). Calibration is offline, so this only affects how long it takes.
- Use a **USB 3** port and cable. At USB 2 the camera's modes are limited (see README.md, *Fixed/Wide Camera*).

## 3. Choose exposure and gain

The board is moved by hand, so keep the exposure short to avoid motion blur: about 2-4 ms. `--exposure` takes the raw V4L2 value; UVC nominally counts in 100 us steps, so 30 is about 3 ms. Raise `--gain` until the image is bright enough.

```bash
python3 src/record.py --seconds 3 --fps 60 --exposure 30 --gain 8 --label test
bin/extractFrame.sh            # frame 90 of the newest test_*.mkv -> ~/at/test.jpg (-f frame, -l label, -o out.jpg)
```

Copy `~/at/test.jpg` back and check:
- the squares have crisp edges
- the whites aren't clipped
- the blacks aren't crushed

Adjust and repeat. Exposure 30 / gain 8 worked indoors on 2026-11-04.

## 4. Record the calibration clip

```bash
python3 src/record.py --seconds 120 --fps 60 --exposure 30 --gain 8 --label calib
```

This writes `~/at/clips/calib_<YYYYMMDD_HHMMSS>.mkv` and a `.json` beside it. In the `.json`, check that `fps_measured` is about 60, `dropped` is near 0, and `misframed` is 0.

`misframed` counts frames that don't start with the JPEG start-of-image (SOI) marker. The camera once produced a clip with its frame boundaries offset (`calib_20261104_153455.mkv`), and `record.py` prints a warning when this happens. Re-record if it does. `calibrate.py` and `bin/extractFrame.sh` can still read such a clip (they skip data before SOI), but `cv2.VideoCapture` can't, and the misframed frames themselves may be lost.

While it records:
- **Move slowly and pause about 1 s at each pose.** Blurred frames are rejected (see `--min-sharpness`).
- **Spend most of the time on the edges and corners.** That is where a wide lens distorts most, and where the fit is weakest without data.
- **Let the board run partly out of frame** at the edges and corners. ChArUco boards work with partial views, so this puts corners right at the image border.
- **Tilt 30-45 deg** left, right, up and down. Don't keep the board parallel to the sensor.
- **Rotate it in its own plane** sometimes: sideways and upside-down.
- **Vary the distance,** about 0.5-1.2 m for the Letter board.
- **Hold it by both side edges**, keeping fingers off the pattern, so it doesn't flex. Flexing shows up as residual arrows that line up within a view (step 6).

## 5. Fit the calibration

```bash
python3 src/calibrate.py ~/at/clips/calib_<YYYYMMDD_HHMMSS>.mkv --board ~/at/board_letter/board.json
```

What it does:
1. Demuxes the clip with GStreamer. It decodes only every `--stride`th frame (default 10, i.e. 6 per second at 60 fps) with OpenCV, skipping any bytes before the SOI marker, and detects the board. It prints how many examined frames were misframed or undecodable.
2. Rejects frames with fewer than `--min-corners` (8) corners, or whose Laplacian-variance sharpness over the board is below `--min-sharpness` (100).
3. Sorts the remaining views into 27 buckets: a 3x3 grid of where the board centre falls, times 3 board sizes. It then picks the sharpest views round-robin across buckets, up to `--max-views` (50).
4. Holds every `--holdout-every`th (7th) selected view out for validation, about 15%.
5. Fits the standard 5-coefficient model with `cv2.calibrateCameraExtended`.

Outputs, in `~/at/calib/` (`--out`, `--name` = `24cug`):

| File | Contents |
|---|---|
| `calib_24cug.json` | K, D, image size, RMS, validation RMS, per-view RMS, train/validation frame numbers, board params, coverage report, field-angle report |
| `coverage_24cug.png` | Every selected corner (green = train, blue = validation) |
| `residuals_24cug.png` | Reprojection-error vectors, scaled x50 (`--residual-scale`) |
| `undistorted_24cug.png` | One frame corrected with K, D |

Copy the results back with `rsync -av orinnano.lan:at/calib/ at/calib/` (or `bin/syncOrin.sh`).

## 6. Evaluate the result

The script prints everything needed to judge the fit. The calibration is **good** when all of these hold:

| Check | Pass | If not |
|---|---|---|
| `RMS` | < 0.5 px (a guide, not a hard limit) | Look at the "worst views" frames it prints (blur, glare, flex) and re-record |
| `validation RMS` | Close to RMS | Much higher means the fit doesn't hold up on new views. Record more varied poses |
| Coverage report | No `SPARSE` zones | Record more poses in the named zones |
| Field-angle report | No `INVALID` probes, no asymmetry `WARNING`s | Usually missing edge/corner coverage. Re-record (step 4) |
| `cy` (and `cx`) | Within ~30-60 px of the image centre (540, 960) | Usually missing coverage at one side |
| `residuals_24cug.png` | Small arrows pointing in random directions | See below |
| `undistorted_24cug.png` | Straight lines straight, including near the borders | See below |

**Coverage report.** This counts board corners in a 3x3 grid of zones. The outer bands are `--edge-frac` (15%) of the width and height. A zone with fewer than `--min-zone-points` (30) corners is flagged `SPARSE`. The report also prints how far across the image the points reach.

**Field-angle report.** The image corners and edge midpoints are converted to viewing angles under the fitted model, and the script reports:
- the horizontal and vertical field of view
- left/right and top/bottom asymmetry (warns above `--max-asym-deg`, 5 deg)
- each probe's angles

A probe is `INVALID` if its ray doesn't project back onto the same pixel. That means the polynomial model is being extrapolated past the data, and the angles there are meaningless. The datasheet field of view is 102.69 x 51.9 deg (horizontal x vertical). A good calibration should come out close to that and roughly symmetric.

**Residual patterns:**
- Arrows **lined up within one view's cluster:** the board flexed or moved during that frame. Hold it more firmly, pause longer, or raise `--min-sharpness`.
- Arrows **consistently pointing outward or sideways near the edges** after the edges are well covered: the 5-coefficient model is too simple for this lens. The next step would be OpenCV's rational model (`CALIB_RATIONAL_MODEL`), which `calibrate.py` doesn't offer yet.

Keep the accepted `calib_24cug.json`. Later stages use its K and D to turn pixels into viewing rays.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `no decodable frames in <clip>` | The clip is empty or corrupt. Check its `.json` (`frames`, `misframed`) |
| `can't open <clip>` | Wrong path. Clips are `~/at/clips/<label>_<date>_<time>.mkv`, so pass the exact file and don't rely on a glob inside quotes |
| `no usable board views found` | `--board` doesn't match the printed board (size/dictionary), or the frames are blurred/dark. Check `test.jpg`, lower `--min-sharpness` |
| `too few usable views` | Fewer than 10 training views survived. Record longer, pause at poses |
| Very few views selected from a long clip | Most frames were blurred. Shorten the exposure, move more slowly |
| Large RMS dominated by a few views | Inspect the "worst views" frames. Glare, fingers or flex are typical |

## Results log

| Date | Clip | Board | RMS / val RMS | Notes |
|---|---|---|---|---|
| 2026-11-04 | `calib_20261104_144038.mkv` (2 min, exp 30, gain 8) | Letter | 0.52 / 0.57 px | **Not usable.** fx 1056, fy 1055, cx 942, **cy 461**. Right ~10% of the image, the bottom strip and 3 of 4 corners uncovered. Corner and right-edge field angles INVALID; vertical field of view lopsided (-25 / +34 deg). Aligned residuals in some views (board held by one corner). Re-record with edge/corner focus |
| 2026-11-04 | `calib_20261104_153455.mkv` (2 min, exp 30, gain 8) | Letter | 1.67 / 1.46 px | **Not usable.** The first frames are misframed (2 misframed and 1 undecodable of 720 sampled). The fit is degenerate: fx 3341, fy 3853, H field of view 34 deg. Very dark (mean brightness ~20/255). Re-record |
