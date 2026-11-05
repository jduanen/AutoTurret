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
python3 src/calibrate.py ~/at/clips/calib_<YYYYMMDD_HHMMSS>.mkv --board ~/at/board_letter/board.json --model rational
```

**Several clips can be pooled.** List them all (`calibrate.py clipA.mkv clipB.mkv ...`, or `bin/doCalibration.sh clipA.mkv clipB.mkv -- --model rational`). Views from every clip go into the same selection. View labels become `<clip>:<frame>`, and the JSON keeps decode stats for each clip. This is meant for topping up coverage: a full clip plus a short clip of just the corners and edges.

**Only pool clips from the same setup:** same focus, lens, mounting and resolution. Pooling `calib_20261105_120503` with `calib_20261104_144038` (recorded the day before) made the fit worse: RMS 0.364 rose to 0.544 px, and corners became INVALID again. Record the top-up clip in the same session, without touching the camera.

What it does:
1. Demuxes the clip with GStreamer. It decodes only every `--stride`th frame (default 10, i.e. 6 per second at 60 fps) with OpenCV, skipping any bytes before the SOI marker, and detects the board. It prints how many examined frames were misframed or undecodable.
2. Rejects frames with fewer than `--min-corners` (8) corners, or whose Laplacian-variance sharpness over the board is below `--min-sharpness` (100).
3. Picks views greedily by the coverage they add, up to `--max-views` (50). The image is a 16x9 grid of 120 px cells. Each step takes the view whose board corners add the most, where a cell is worth 1 / (1 + times already covered), with sharpness breaking ties. Empty cells, usually the corners and edges, dominate until they're filled. It prints how many of the 144 cells the selection covers.

   This replaced a selection by board centre (3x3 grid x 3 board sizes), which kept losing partial views that reach into a corner to larger, more central ones. On the 11-05 clips, 24 usable top-right and 12 bottom-left frames went unused that way.
4. Holds every `--holdout-every`th (7th) selected view out for validation, about 15%.
5. Fits the distortion model chosen with `--model` using `cv2.calibrateCameraExtended`:
   - `standard` (default): k1, k2, p1, p2, k3
   - `rational`: adds k4, k5, k6 (`CALIB_RATIONAL_MODEL`)

   **Use `--model rational` for the 24CUG.** On this 102 deg lens, the standard polynomial folds back about 56 deg off-axis. It then can't represent the right edge or the corners at all, whatever the coverage. The rational model fits better (lower RMS, validation RMS close to it) and stays invertible at the corners; see the results log. The JSON records the model, and `D` has 5 coefficients (standard) or 14 (rational; the unused ones are 0). OpenCV's `undistort` and `projectPoints` take either.

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
- Arrows **consistently pointing outward or sideways near the edges** after the edges are well covered: the distortion model is too simple for this lens. Use `--model rational`. If that still leaves edge residuals or `INVALID` probes with good coverage, OpenCV's fisheye model is the next step; `calibrate.py` doesn't offer it yet.

Keep the accepted `calib_24cug.json`. Later stages use its K and D to turn pixels into viewing rays.

## Distortion models

Every model starts from the ideal pinhole projection of a 3D point, x = X/Z, y = Y/Z, with r² = x² + y². Note that r = tan(θ), where θ is the angle off the optical axis. The distortion model moves that ideal point to where the lens actually puts it, (x_d, y_d). K then converts it to pixels: u = fx·x_d + cx, v = fy·y_d + cy.

**Standard (`--model standard`, 5 coefficients k1, k2, p1, p2, k3)**

x_d = x · (1 + k1·r² + k2·r⁴ + k3·r⁶) + tangential terms (p1, p2)

The radial part is a polynomial. Barrel distortion needs a negative k1, so the distorted radius r_d = r·(1 + k1·r² + …) rises, peaks, and then falls. Past the peak, two different angles map to the same pixel and the model can't be inverted. Those are the `INVALID` probes in the field-angle report.

On the 24CUG (2026-11-05 clip), the standard fit peaks at r = 1.48 (56 deg off-axis), where r_d = 0.918. The left edge needs r_d = 0.878, which is reachable. The right edge needs 0.939 and the corners 1.00-1.09. Those pixels can't be represented by this model at all, whatever the coverage.

**Rational (`--model rational`, adds k4, k5, k6; `CALIB_RATIONAL_MODEL`)**

x_d = x · (1 + k1·r² + k2·r⁴ + k3·r⁶) / (1 + k4·r² + k5·r⁴ + k6·r⁶) + tangential terms

The ratio of two polynomials can level off instead of folding back, so r_d keeps increasing slowly with angle, as a real wide-angle lens does. On the same clip it reaches the corners (59-64 deg off-axis) and fits better: RMS 0.364 px vs 0.415, validation 0.375 vs 0.491.

Notes:
- **Individual coefficients aren't meaningful.** The fit gave k1 = 2.09 and k4 = 2.47. The numerator and denominator largely cancel, and only their ratio matters. Don't compare coefficients between models.
- **`D` has 14 entries:** k1, k2, p1, p2, k3, k4, k5, k6, then thin-prism s1-s4 and sensor-tilt τx, τy. Only the first 8 are fitted; the rest are 0. `cv2.undistort`, `undistortPoints` and `projectPoints` accept any of these lengths.
- **More parameters, more risk of overfitting.** Check that validation RMS stays close to fit RMS.
- **It still extrapolates past the data.** Beyond the last board corners it can go anywhere.

**Fisheye (not implemented)**

OpenCV's fisheye model works on the angle θ directly, θ_d = θ·(1 + k1·θ² + k2·θ⁴ + …), instead of r = tan θ. It's meant for lenses beyond ~120 deg, where tan θ becomes very large. Try it only if the rational model still leaves `INVALID` probes or edge residuals with good edge and corner coverage. It needs a separate API (`cv2.fisheye.calibrate`), which `calibrate.py` doesn't use yet.
## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `no decodable frames in <clip>` | The clip is empty or corrupt. Check its `.json` (`frames`, `misframed`) |
| `can't open <clip>` | Wrong path. Clips are `~/at/clips/<label>_<date>_<time>.mkv`, so pass the exact file and don't rely on a glob inside quotes |
| `no usable board views found` | `--board` doesn't match the printed board (size/dictionary), or the frames are blurred/dark. Check `test.jpg`, lower `--min-sharpness` |
| `too few usable views` | Fewer than 10 training views survived. Record longer, pause at poses |
| Very few views selected from a long clip | Most frames were blurred. Shorten the exposure, move more slowly |
| `reading <clip> stalled for 60 s` | Storage stall. The Orin's NVMe occasionally loses a completion interrupt, and the read hangs until the kernel's 30 s `io_timeout` (`journalctl -k`: `nvme ... timeout, completion polled`). The reader waits up to 60 s, so this means something worse. See README.md, *Orin storage* |
| Large RMS dominated by a few views | Inspect the "worst views" frames. Glare, fingers or flex are typical |

## Results log

| Date | Clip | Board | RMS / val RMS | Notes |
|---|---|---|---|---|
| 2026-11-04 | `calib_20261104_144038.mkv` (2 min, exp 30, gain 8) | Letter | 0.52 / 0.57 px | **Not usable.** fx 1056, fy 1055, cx 942, **cy 461**. Right ~10% of the image, the bottom strip and 3 of 4 corners uncovered. Corner and right-edge field angles INVALID; vertical field of view lopsided (-25 / +34 deg). Aligned residuals in some views (board held by one corner). Re-record with edge/corner focus |
| 2026-11-05 | `calib_20261105_120503.mkv` (2 min, exp 30, gain 8) | Letter | standard 0.415 / 0.491 px | **Not usable at the edges.** Edges now covered (x 4-96%, y 5-96%), but the corners are still empty (TR 0, BL 0, BR 1, TL 15 points). fx 1055.4, fy 1056.8, cx 928, cy 502. The standard model folds over at 56 deg off-axis, so the right edge and all corners are INVALID |
| 2026-11-05 | same clip, `--model rational` | Letter | **0.364 / 0.375 px** | **Better model; still needs right-edge and corner data.** fx 1052.7, fy 1052.3, cx 926.5, cy 501.0. All corners valid (59-64 deg off-axis), but the right-edge midpoint is still INVALID (+61.2 deg), with L/R asymmetry +9.1 deg. Both models agree where there is data (left edge -52 deg, top -27.5, bottom +31.8). Re-record with the right edge and corners as the focus |
| 2026-11-05 | `120503` + `144038` pooled, `--model rational` | Letter | 0.544 / 0.442 px | **Worse than 120503 alone.** fx 1065.6 vs 1052.7. Top-left, bottom-left and right edge INVALID again; only 4 of 50 views came from 144038. The clips are from different sessions (focus/mount likely changed), so don't pool them |
| 2026-11-05 | `120503` + `134015` (same session; 2 min + 1 min), `--model rational`, centre-bucket selection | Letter | 0.417 / 0.360 px | **All probes valid, asymmetry 3.2 / 4.3 deg.** fx 1052.3, fy 1052.5, cx 932.4, cy 502.3. Right edge +55.6 deg. But the selection left the corners empty (TL 15, TR 0, BL 0, BR 1 points) although the clip had usable TR/BL views, and the right-edge residuals pointed outward |
| 2026-11-05 | same clips, **coverage-based selection** | Letter | **0.403 / 0.347 px** | **Best so far.** fx 1054.4, fy 1050.4, cx 929.5, cy 499.5. Covers 143/144 grid cells; points span x 2.5-97.5%, y 5.9-96%. Corner zones TL 23, TR 9, BL 10, BR 5 points (still below the 30-point SPARSE threshold). All probes valid; H field of view 108.4 deg (asym +3.6), V 59.6 deg (asym +4.6). Residuals random everywhere, including the right edge. Still needed: more extreme-corner data, especially bottom-right |
| 2026-11-04 | `calib_20261104_153455.mkv` (2 min, exp 30, gain 8) | Letter | 1.67 / 1.46 px | **Not usable.** The first frames are misframed (2 misframed and 1 undecodable of 720 sampled). The fit is degenerate: fx 3341, fy 3853, H field of view 34 deg. Very dark (mean brightness ~20/255). Re-record |
