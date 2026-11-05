# AutoTurret

This project involves the creation of a standalone, autonomous, system that is capable of detecting and tracking airborne and ground-based objects.

## Initial Assumptions/Requirements/Constraints

A phased approach will be taken by this project, where each phase develops and refines solutions to a subset of the overall problem, and build successively in capability.

The initial design requirements are derived from the hummingbird detection and tracking example use case. This was chosen because of the technical challenges that stem from such small, fast-moving, targets (and because of the abundance of hummingbirds in my backyard that can be used in the development process).
Furthermore, a turret that can detect and track hummingbirds should be capable of doing the same for various other types of targets, e.g., aircraft, squirrels, etc.).

Another initial design decision is to start with an azimuth and elevation platform on a fixed base. The fixed base can contain a set of (typically wide field of view (FOV)) sensors that can be used to aid target detection, and the AzEl platform carries (more focused) sensors that are used to direct the AzEl platform to track the target(s).

Initially, a method of target detection will be developed
tracking of a single target will be

* single target then multiple targets
* modular design -- allow modification/enhancement to blocks (more or less) independently -- e.g., add new sensor types, and other capabilities
* start with simple/light/small/low(ish) power payload, then add additional sensors/actuators to the mobile platform
* use Teknic ClearPath servos -- lots of speed/acceleration/torque, small package, good API for development, high resolution/repeatability
* will start plugged into mains, will explore making solar+battery power source later
* gather lots of telemetry on the platform -- position/power/temperature/etc., detailed local weather information, etc.. can be used by features beyond just tracking
* look at multi-modal sensors to aid in detection and tracking -- audio, video, thermal, distance, etc.
* initially, can ship sensor data to a local server (over WiFi) and send motion commands back, but real desire is to have autonomous operation where all of the processing functions are handled on the platform. will still like to have WiFi link to stream (low latency) video for development, logging, and other reasons
* assume we will start with a fixed (high res, wide FOV) camera on the base and a (telephoto) camera on the mobile platform
* implement a type of vision-servo tracking system

## Background Information

* Hybrid SW approach
  - classical motion processing and geometric control for speed
  - learned detection for target recognition
  - temporal tracking to connect observations

* Target Detection and Tracking
  - Detection: locates and object in an image
  - Classification: assigns a category to a detected object
  - Tracking: links observations over time and estimates future motion; assigns IDs to tracked objects
  - Recognition: distinguishes categories, and in the limit, individual identities

### Example Targets Types and Their Key Characteristics

  - (Anna's) Hummingbirds
    * emit sounds
      - wing hum: 40-45Hz
        * plus harmonics at 80-90Hz, 120-135Hz, up to 400Hz
        * mean wingbeat frequency measured at 44.2 +/-1.8Hz
        * changing lift/drag produces series of harmonics that create the buzz sound
      - male songs: 3-6kHz
        * rapid trill spanning 3.0-5.75kHz and subsequent note spanning 3.25-5.0kHz
        * Doppler shift of up to +/-300Hz
      - ordinary "chip" calls: brief broadband sounds
      - courtship dive chirp: 4.1kHz +/-0.2kHz (with higher harmonics)
    * high speed/acceleration/rate of change
      - courtship dive
        * 27.3 +/-3.2 m/s (avg) speed
        * 8.5-9 g (83-88 ms^2) pullout from dive
      - horizontal flight: 15 m/s (peak), 2.22 m/s (avg around feeder)
        * 1.20-2.94 m/s (measured maneuver peak speeds)
      - short horizontal acceleration
        * measured: 6.30 m/s^2 (avg), 2.96-8.83 m/s^2
        * horizontal breaking: 6.67 m/s^2 (mean maneuver peak)
    * video tracker suggestions for hummingbirds
      - RasPi with fixed global-shutter camera
      - record timestamped 60 FPS clips
      - develop motion detector and tracker offline before realtime inference
    * thermal footprint: ????

  - Drones
    * emit sounds (motor/jet/prop noise)
    * may emit radiation (RF links)
    * assume that they are both larger, slower, and lower acceleration than hummingbirds

  - Backyard Critters (squirrels, raccoons, possums, cats, dogs, rats, etc.)
    * make few sounds
    * ground-based, but some are capable of big leaps
    * larger than hummingbirds and not as fast
    * warm-blooded (have a detectable thermal footprint)
    * ?
 
### Vision-Servo Tracking Systems

  - for rectilinear camera, approximate projected body length near the image center
    * p ~ (W * L) / (2 * d * tan(theta / 2))
      - W: horizontal resolution
      - L: projected length perpendicular to the viewing direction
      - d: distance
      - theta: horizontal FOV
    * as a target, want to get 25-50 pixels within the tracking region
  - at 10 m/s, object travels ~170mm between frames at 60 FPS
  - don't have to run detector on every full-resolution frame
    * can run frequent light-weight motion tracking with less frequent learned detections to confirm or correct tracks
      - long gap between detections is difficult due to abrupt maneuvers
      - small-bird tracking literature identifies rapid motion and association failures as coupled problems
  - how fast does a PZT mount have to move
    * approach as visual-servo system
    * biggest problem is latency -- must do detection, motion prediction, and move before object leaves the FOV
    * works better for following an acquired target, as opposed to continuously detecting all targets in the FOV
    * target's angular speed relative to the camera is important (not just linear speed)
    *  w ~ v_p / r
      - w: angular speed
      - v_p: velocity perpendicular to the line of sight
      - r: range
    * e.g., 10 m/s target speed at 10 m from camera -- 57 deg/s
      - gets more demanding to track the closer the object is from the camera
      - at the same time it gets harder to detect the further the object is from the camera
    * e.g., 57 deg/s angular speed, with total delay of 100ms, object moves 5.7 deg in that time
      - if 10 deg HFOV, then object will leave the frame before the system can respond
    * prediction helps, but is defeated by rapid changes in direction
      - prediction requires both speed and robustness
      - prediction has to account for target motion, camera motion, and delays
      - predict where the target will be when the mount arrives
      - basic angular prediction
        * thetaHat(t + T) = theta(t) + thetaDot(t)T
          - T includes frame age, inference time, comm, and mount response
        * e.g., target moving at 60deg/s moves 6deg in 100msec, so acquisition FOV must accommodate both this motion and prediction uncertainty
        * processing flow
          - timestamp observations at point of capture
          - measure end-to-end delay (don't infer from frame rate)
          - avoid processing buffered stale frames
          - keep uncertainty margins around the predicted target position
          - let PZT camera correct residual pointing errors after handoff
      - predictive visual-servo research combines target-state estimation with FOV constraints

  - fundamental tradeoffs
    * wide FOV: easier target acquisition and retention, fewer pixels on target
    * narrow FOV: better target detail, less tolerance for pointing error and latency
    * aggressive zoom: improved detail only while tracking remains stable

  - discard late frames, don't let frames get buffered

  - control angular error with prediction
    * controller combines
      - angular position error for centering
      - estimated target angular velocity as feed-forward
      - prediction over measured feedback delay
      - rate and acceleration limits
      - modest central deadband to reduce jitter
    * estimate target motion in a camera-motion-compensated coordinate system
      - otherwise, the mount's movement will look like target movement

  - handle target acquisition and loss explicitly
    * state machine: Search -> Acquire -> Track -> Lost -> Reacquire/Search
    * on loss, briefly search near the predicted direction, then widen view (if possible, else scan)

  - important HW specs
    * pan/tilt speed with full payload
      - determines how well the system can handle close passes
    * angular acceleration and settling behavior
      - determines response to target turning/changing direction
    * command-to-motion latency
      - part of the tracking delay budget
    * encoder feedback and timestamps
      - helps separate camera motion and target motion
    * backlash, vibration, and pointing repeatability
      - affects image stability and control
    * continuous velocity control
      - enables smoother visual servoing
    * cable routing and travel limits
      - prevents interruptions during extended pans

  - log target image position, mount angles, frame age, and command timing
    * to distinguish detector failures from control-loop failures

### Servos

* Servos for AzEl platform
  - assume 200-300g payload, compact and well balanced cradle
  - specs at P&T axis (not motor shaft)
    * max tracking/slew speed: P = T = 300 deg/s
    * angular acceleration:    P = T = 3,000 deg/s^2
    * peak torque:             P = 0.3--0.6 N-m, T = 0.15-0.3 N-m
    * continuous torque:       P = 0.1--0.2 N-m, T = 0.05-0.15 N-m
    * angular resolution:      P = T = 0.001--0.01 deg
  - pan/tilt geometries typically have a singularity at the directly overhead position
  - need to define the operating envelop in terms of what it can and can't do
  - acceleration matters as much as top speed
    * at 3000 deg/s^2, accelerating from rest to 300deg/s takes 100ms
    * reversing from 150deg/s to -150deg/s also takes 100ms
    * an ideal 90deg acquisition takes about 400ms, including acceleration and breaking times
  - real motion takes even longer when jerk limits, structural vibration, controller delay, and settling are taken into account
  - most meaningful spec is loaded speed-torque capability with actual payload inertia
  - calculate torque separately for each axis
  - pan axis doesn't create a direct driving torque
    * but it has to move the whole assembly, so the inertia will be greater than the tilt axis'
  - adding weight to balance payloads results in higher inertia
  - resolution specs
    * Command Resolution: smallest angle increment that the controller accepts
    * Encoder Resolution: smallest reported angular increment
    * Actual Positioning Accuracy: difference between the requested and physical angle
    * Backlash/Repeatability: how consistently the axis responds, particularly after reversing direction
  - for 20-30deg PZT view
    * Output feedback resolution: 0.005 deg or better
    * Low-speed pointing repeatability: 0.02 deg or better
    * Reversal backlash/deadband: below 0.02 deg
    * Stationary angular jitter: 0.01 deg RMS or less
    * 16-18bit encoder

==> put references to the ClearPath repo/docs here
  - I want to use two CPM-SCSK-2311S-EQNA (or EQSA) servos for the axes
    * enhanced resolution, 1/4" shaft, advanced feature set
      - prefer liquid-tight seal, but can live with standard dust seal
    * $429

  - mechanical transmission
    * provisional requirements
      -  300°/s maximum axis speed
      -  3,000°/s² acceleration
      -  0.005–0.01° command increments at the turret output
    * current strawman approach
      - use 9mm GT2 timing belts -- e.g., 400mm pre-made loop
      - 10:1 drive ratio, 16-tooth pulley on motor, 160-tooth pulley on axis
        * need at least 6-tooth wrap -- use idlers if necessary
        * want to minimize the distance between the pulleys
    * gears have backlash issues
  - bearings
    * Az axis
      - a trunion, can use standard bearings
    * El axis
      - need to handle loads outside of the rotational axis
        * need something like 200mm cross-roller bearings
          - very big, heavy, and expensive
      - lazy-susan are poor in terms of accuracy and longevity
      - has to handle rapid acceleration/deceleration and high-speed operation (but not sustained)
  - electrical power transmission
    * Az axis: ?
    * El axis: ?

### Computing Resources
  - on-board vs. remote
  - control-loop processing probably has to be local for latency reasons
  - can remotely provide higher-level supervision and guidance (with longer time constants)
  - can use RasPi 4/5, NVDA Tegra/Orin Nano/etc.
    * start with Orin Nano Super and See3CAM_24CUG fixed camera, ??? mobile camera
  - ????

### Cameras

* one or more fixed-FOV cameras and a PZT camera
  - processing loop
    * fixed wide-FOV camera(s) detects targets and maintains wide-view tracks
    * select target and pointing direction from wide view
    * slew PZT camera toward target
    * acquire target with PZT camera
    * close-up visual tracking controls PZT
      - classification and recording done from PZT camera video
    * if target lost, reacquire using wide-FOV camera(s)
  - two major operating modes
    * sensor input for target detection: DETECTION mode
      - can be the wide-FOV camera, lidar, thermal, other, or combinations
    * wide-FOV camera(s) guidance for initial pointing and recovery: ACQUISITION mode
    * PZT-camera feedback for precise centering of acquired target: TRACKING mode
  - have to figure out which object to track if more than one detected
    * use some characteristic of the detected targets (e.g., bounding box size, distance, etc.), from the wide-FOV cameras or other sensors to choose target to track
      - could be based on the target's absolute location, direction, speed, size, or relation to the other detected targets
    * could also just keep tracking the object with the longest track history
  - fixed cameras have to be mounted as close to the PZT's optical center as possible
    * wide-view camera detection gives a viewing ray, not a distance
    * if cameras are separated the ray doesn't specify where to focus the camera without knowning the range to the target
      - parallax angle for transverse camera separation 'b' and target distance 'r'
        * deltaTheta ~ (b / r)
  - calibration process
    * calibrate wide-cameras' intrinsics and lens distortion
    * establish wide-cameras' orientation relative to the mount
    * measure pan/tilt zero offsets and actual angle response
    * calibrate focused camera at the chosen focal length (if PZT camera is fixed-focus)
    * check pointing accuracy with stationary targets -- both near and far ones
  - for a calibrated rectilinear camera, an undistorted pixel can be converted into a viewing ray
    * q -> K^-1 [u v 1]^T
    * rotate that ray into the mount's coordinate system, then derive the desired P&T angles
    * for a fisheye lens, use its calibrated project model instead
  - start with PZT camera fairly wide (e.g., 20deg) then narrow zoom as tracking improves
  - doesn't provide range information
    * can get it by using stereo PZT cameras or another sensor (lidar, RF, etc.)

* Fixed/Wide Camera
  - See3CAM_24CUG (e-con systems, India)
    * color global shutter camera
    * on-board ISP
    * M12/S-mount
    * Sensor: onsemi AR0234CS
    * Interface: USB-C
    * Lens: 2.8 mm
    * Field of view: 102.69° horizontal × 51.9° vertical
      - for plane perpendicular to the optical axis
        * W = 2*d*tan(102.69/2)
        * H = 2*d*tan(51.9/2)
      - field boundaries: (distance: width, height)
        * 3m:   7.5m, 3.9m
        * 5m:  12.5m, 4.9m
        * 10m: 25m,   9.7m
      - pixels are not uniformly distributed over the area -- must deal with distortion
    * Chief ray angle: Less than 8°
    * Exposure: selectable/manual and automatic
    * Gain: selectable/manual and automatic
    * Outputs:
      - UYUV:  1280 × 720  @ 80 fps
      - UYUV:  1920 × 1080 @ 60 fps
      - MJPEG: 1280 × 720  @ 100 fps
      - MJPEG: 1920 × 1080 @ 100 fps
    * Formats actually enumerated on Orin (v4l2-ctl, 2026-11-04, camera linked at USB 2.0/480M):
      - UYVY:  1280 × 720 @ 15, 1920 × 1080 @ 8, 1920 × 1200 @ 6 fps
      - MJPEG: 1280 × 720 @ 120/60, 1920 × 1080 @ 120/60/30, 1920 × 1200 @ 114/60 fps
      - must be on a USB 3 port/cable for full UYVY rates (and sustained MJPEG at 120fps)
      - measured on USB 3 (record.py, 10s): 1080p60 MJPEG = 16.65ms mean interval, 1 drop;
        1080p120 MJPEG reaches 119-120fps only with a queue after v4l2src (otherwise ~110fps); even with raw
        v4l2-ctl capture, some exposure/gain combos run at 107-116fps (e.g., exp 20/gain 4), with no clear
        pattern -- camera-side (USB 3 bulk, burst 15; not frame size). Check fps_measured in record.py's json
    * MJPEG quality: selectable quality factor
  - See3CAM_24CUG is the camera for initial testing (Phase 1)
  - Candidate for wide-area detection: Arducam B0397 (12MP*4 Quadrascopic Camera Bundle Kit)
    * https://www.arducam.com/12mp4-quadrascopic-camera-bundle-kit-b0397.html
    * Sensors: 4x Sony IMX477, 1/2.3", ROLLING shutter
    * Lenses: M12, 3.9mm, F/2.8, ~75deg H x 56deg V each, manual focus, IR-cut
    * Interface: UC-512 CamArray HAT, all four cameras multiplexed onto one MIPI CSI-2 port
      - Orin Nano Super dev kit has two CSI ports -- leaves one free for the PZT camera
      - JetPack 5.x+ needs CamArray firmware >= 2023.02.03; official carrier board only
    * Quad-mode output: one combined frame containing all four images
      - 4056x3040 combined (2028x1520 per camera) at ~10fps
      - 1920x1080 combined (~960x540 per camera) at half the normal frame rate
    * Sync: cameras 0/1 synchronized, 2/3 synchronized, 2/3 one frame behind 0/1
      - need per-pair timestamp correction
    * No ISP support on Jetson -- expect RAW Bayer; debayer/AE/AWB must be done in SW [confirm with Arducam]
    * ~$400 (EUR 359)
  - B0397 concerns for hummingbird detection
    * rolling shutter -- skew/smear on fast wings and dives, hurts motion detection and pixel-to-ray calibration
    * low frame rate in quad mode -- at 20fps a 10m/s target moves 0.5m between frames (plan assumes >= 60fps)
    * four separated optical centers -- worsens parallax vs. turret, and requires four intrinsic + extrinsic calibrations
  - B0397 advantages
    * ~180-270deg coverage (with overlap) vs. 102deg for one 24CUG
    * interchangeable M12 lenses allow trading FOV for pixels per camera
  - pixels on target -- 100mm body at 4m (p ~ (W * L) / (2 * d * tan(theta / 2)))
    * 24CUG, 1920 wide, 102.69deg HFOV: ~19 px
    * B0397, 2028 wide per camera, 75deg HFOV: ~33 px (only in slow full-res mode)
    * B0397, ~960 wide per camera, 75deg HFOV: ~16 px
  - alternatives to evaluate before committing
    * Arducam global-shutter multi-camera kits for Jetson (e.g., OV9281 1MP mono quad kit, check for AR0234-based options)
    * multiple 24CUGs over USB (each has its own ISP)

* Focused/Mobile Camera
  - MIPI-CSI to direct connect to Orin Nano Super
    * must be one with driver available
  - Lens: ????
  - ????

### Detection
  * multi-modal sensors (visual, acoustic, thermal, RF, Lidar, etc.)
  * example pipeline
    - Fixed-camera acquisition: identify motion candidates across the yard
    - Candidate classification: run a bird detector on image crops rather than shrinking the entire yard image until the birds disappear
    - Predictive hand-off: convert the fixed camera’s bearing into a turret command, accounting for calibration, camera separation, and latency
    - Focused-camera tracking: use the turret camera’s image error and predicted velocity to keep the bird centered
    - Buffered recording: retain footage from before detection through a short period after losing the target
    - Separate motor control: let Orin Nano (or dedicated uCtlr) handle the fast actuator loop
      * keep vision and trajectory estimation on the Orin

### Tracking

* Classic Vision and ML Tracking techniques
  - optical flow
    * estimates local image movement between frames
    * detects all motion, not just that of intended targets
  - template correlation/tracking
    * follows initialized image patch without repeated category detection
    * can drift or lose the target when appearance changes
  - Kalman prediction plus assignment
    * predicts motion and associated multiple detection efficiently
    * struggles with abrupt turns and long gaps in observation
  - learned appearance embeddings
    * tiny, blurred, similar-looking targets offer weak identity cues
  - learned visual trackers
    * learn how to match a target across frames
    * need testing for small targets, occlusion, and specific backgrounds
  - classical motion is useful for fixed wide-FOV cameras' flow
    * Background/motion candidates
    * Native-resolution candidate crops
    * learned target detector
    * motion prediction and detection association
    * target selection and predicted pointing direction
  - inexpensive scene-wide detection and only spend ML compute on likely locations
    * still need to deal with stationary targets
  - background subtraction is not useful when the PZT camera moves
    * use learned detection to acquire targets
    * use a local tracker between detections
    * encoder feedback or image registration to account for camera movement
    * geometric conversion from image error to P&T commands
    * use wide-FOV camera guidance when close-up track fails
  - empirically compare different pipelines
    * motion detection plus classical association
    * learned detection plus classical association
    * motion-guided learned detection plus classical association
    * the best of the above approaches, with a faster local tracker between detections
  - measure
    * target detection recall: how often are visible targets found
    * false positives per minute
    * track fragmentation and ID switches
    * localization error: important for pointing the PZT camera
    * frame age when the control command is issued
    * acquisition time and percentage of time the target remains focused in view
  - make sure that training is done in different environments so it doesn't overfit on the specific locale
  - instead of an end-to-end learned system, start with a hybrid approach:
    * classical motion proposals on the fixed camera
    * a learned target detector for confirmation
    * classical state estimation and association for track continuity
    * calibrated geometric pointing and a measured-latency servo loop
    * close-up tracking with periodic detector correction

* can't count on appearance-based re-identification for hummingbirds or drones
  - can track a target while it remains in view
  - hard to determine if it's the same one or a different one if a target goes out of view and then returns

### Video Streaming
  * all payloads include video sensor
    - variable focus? digital/analog zoom?
  * low latency video capture for local processing (for local vision tasks)
  * video streamed over WiFi with low latency

### Payload
  * ????

### Telemetry
  * Store local history (FIFO ring buffers for limited look-back)
  * Publish (selected) information as MQTT messages over WiFi
  * Information gathered
    - Base Location/Orientation
       * absolute, repeatable, accurate (IMU, magnetometer)
    - Turret Position (relative to Base)
    - Environmental Conditions
      * Temperature (of key components and ambient)
      * Air Pressure
      * Wind speed and direction
      * Rain
    - Power (voltage and current at different points)

## Development Phases

### Phase 0: Information Gathering and Preparation

- literature search
- commercial products
- open-source implementations
- list problems to be solved and unknowns
- define phases to solve each problem
- select candidate technologies to explore in each phase
- identify goals and success criteria for each development phase

### Phase 1: Target Detection

* camera calibration
  - step-by-step procedure, acceptance criteria, and results log: see CAMERA_CALIBRATION.md
  - measure intrinsics: lens's pixel-to-viewing ray mapping and distortion
    * make OpenCV ChArUco board
      - 7x5 squares, 50mm squares, 35mm marker side
      - Dictionary: DICT_5x5_100
      - overall patterned area: 350x250mm plus white margin
      - print actual size on matte paper and mount flat on rigid backing
      - measure printed dims to be sure
    * use OpenCV ChArUco board
      - estimates camera matrix and distortion from multiple views of the board
      - capture at 1920x1080, uncompressed, 60fps
        * save selected still frames
      - locked focus at final focus setting
      - exposure short enough that the calibration board is sharp
      - gain: enough to have clear, well-defined corners of the board
      - keep crop, digital zoom, and geometric correction settings unchanged
      - move board until fills view, but in focus, and corners stay focused
        * use larger board farther away rather than refocusing the lens just for calibration
      - capture varied board poses across the whole image
      - fixed camera, move the board manually
      - collect 30-50 sharp images
        * center: several sizes and distances
        * left/right/top/bottom: board corners distributed near each image boundary
        * four image corners: enough coverage to constrain edge distortion
        * in-plane rotations: several different rotations of the board
        * include substantial tilt -- don't keep parallel to the sensor in every shot
        * cover the edges not just the middle
        * hold the board steady before snapping each image
        * reject blurred, glared, or poorly detected views
        * set aside several good images for validation dataset
  - calculate orientation relative to the turret and environment
    * can come later in the process, before generating motion commands
    * converting target pixels into axes bearings (with calibrated camera
      - don't have to undistort every frame
      - first get camera-relative angles, and then turret motor commands
      - calibration to get fixed-camera-to-turret-transform
  - fit camera model in OpenCV (see OpenCV tutorial)
    * for each image
      - detect ChArUco corners and IDs
      - match corners to board coordinates
      - append object points and image points
    * run calibrateCameraExtended()
    * save K, D, image dimensions, and per-view erros
  - first fit standard model and look at edge residuals
    * only if can't adequately describe the image with the standard model, use richermodl
  - validate geometry, not just reported RMS
    * smaller reprojection error is better
      - but spatial pattern of the error also matters
    * aim for overall RMD < 0.5 pixel (not hard pass/fail criteria)
    * inspect per-view errors and investigate bad image outliers
    * plot residual vectors across the image
      - systematic outward or sideways errors near edges suggest an inadequate fit
    * inspect an undistorted image of straight features, including near the edges
    * work hard at characterizing the difficult edge views -- don't discard them
* set up controlled scene
  - near feeder -- 3-5m
  - center expected activity in camera's FOV
  - try to get steady and reasonable amount of illumination
  - ensure rigid mount and set focus properly
  - avoid background with (wind-driven) motion or backlit
* make baseline recordings -- no inference, focus on repeatability
  - full HD, as fast framerate as possible >=60fps
  - Exposure: start with 0.5 msec (1/2000 sec)
  - Gain: fixed, adjusted for usable brightness
  - turn off auto exposure/gain once adjusted properly to avoid changing during tests samples
  - record several (10-20) short clips, plus null clips -- positive and negative examples
* manually review clips at native resolution
  - for ~dozen sequences record:
    * distance: approximate range from camera to bird
    * target size: bounding box width/height in pixels, (body only, record wing extent separately)
    * contrast: determine if the body separates from the background
    * blur: degree to which body is smeared in a frame
    * persistence: number of consecutive frame that has a given bird
    * background: note type -- sky, foliage, fence, feeder, etc.
* run a simple motion-candidate detector offline
  - using recorded full-res frames
    * grayscale
    * background subtraction: e.g., OpenCV MOG2
    * foreground connected components
    * candidate boxes and centers
    * short temporal associations
    * measure overlay video and detection lag
* manually score the result
  - how many correct candidates generated (out of all known positives)
  - false positives per minute (including null scenes)
  - track continuity -- gaps in detection?
  - pixel size vs. successful detection -- which sizes work
  - aim for 9/10 correct detections as first milestone
* responses for possible outcomes
  - missed detection of clearly visible target: adjust background adaptation, thresholds and component filtering
  - blurred target: shorten exposure, ensure adequate lighting
  - get detections, but background dominates: add temporal filtering before candidate classifier
  - works at 1080p60: try again with high-Q 1080p100 MJPEG
* outputs from this phase
  - camera calibration
  - video capture device
  - video capture processing pipeline
  - small labeled dataset
  - measured target pixel sizes
  - realtime motion-detection video overlay
  - decision on UYUV vs. MJPEG
* software (src/, runs on the Orin with system python3/OpenCV 4.8/GStreamer -- see requirements.txt)
  - deploy: `rsync -a src/ orinnano.lan:~/Code/AutoTurret/src/`
  - outputs default to ~/at/ on the Orin
  - camera.py: See3CAM_24CUG device lookup, manual exposure/gain/WB (raw V4L2 values), GStreamer source
  - make_charuco.py: generate the 7x5/50mm/35mm DICT_5X5_100 board PNG (300dpi) and board.json
    * `python3 src/make_charuco.py` (README board, 50/35mm -- needs 11x17/A3)
    * `python3 src/make_charuco.py --square-mm 35 --marker-mm 24.5 --out ~/at/board_letter` (fits US Letter landscape)
    * PDF at exact scale on a Letter page: `magick charuco_7x5_35mm.png -background white -gravity center -extent 3300x2550 -units PixelsPerInch -density 300 charuco_7x5_35mm_letter.pdf`
    * calibrate.py takes `--board ~/at/board_letter/board.json` for the Letter board
  - record.py: MJPEG passthrough clips (.mkv, no re-encode) plus .json with settings, control readback, frame timing, fps_measured, dropped frames (from capture-timestamp gaps; driver sequence numbers miss frames the camera never sent), and misframed frames (not starting with JPEG SOI -- warns; seen once)
    * `python3 src/record.py --seconds 20 --fps 60 --exposure 5 --gain 4 --label feeder`
  - calibrate.py: ChArUco intrinsics from record.py clip(s) -- selects views greedily by the 16x9-grid coverage they add (sharpness breaks ties), holds out ~15% for validation
    * reads clips with GStreamer demux + cv2.imdecode of only every --stride-th frame (skips data before SOI, so misframed clips still work); ~100 fps of clip scanned
    * accepts several clips (views pooled; labels <clip>:<frame>) -- only pool clips from the same focus/mount session
    * --model standard|rational (default standard) -- use rational for the 24CUG: the 5-coefficient model folds over ~56deg off-axis and can't reach the right edge/corners (see CAMERA_CALIBRATION.md)
    * `python3 src/calibrate.py ~/at/clips/calib_<ts>.mkv`
    * writes calib_24cug.json (K, D, RMS, validation RMS, per-view errors) and residual/coverage/undistorted PNGs
    * prints/saves a coverage report (board-corner counts in 3x3 zones, outer 15% bands; SPARSE zones flagged)
      and field angles of the image corners/edge midpoints (H/V FOV, asymmetry, INVALID where the fitted
      model doesn't invert -- i.e., extrapolated past the data); a usable calibration has no SPARSE or INVALID
  - motion_detect.py: MOG2 motion candidates, connected components, greedy nearest-neighbour tracks
    * `python3 src/motion_detect.py ~/at/clips/feeder_<ts>.mkv` (offline) or `python3 src/motion_detect.py --live --seconds 10` (camera)
    * writes <clip>_overlay.avi, <clip>_candidates.csv (frame, timestamp_ms, track_id, track_hits, box, area), and <clip>_summary.json (processing time, live frame age)
    * green boxes = tracks with >= --min-track-len hits, orange = unconfirmed candidates
    * --max-candidates (default 50) keeps the largest blobs per frame; summary frames_capped > 0 means a noisy scene/thresholds
    * live mode with the overlay video manages only ~15fps (overlay encode is CPU-bound) -- use --no-video for timing runs
    * --cv-threads (default 4) limits OpenCV's worker threads -- with all 6 Orin cores, MOG2 starves the jpegdec
      thread and live 1080p60 processes only ~26-33fps; with 4 it processes ~49-51fps (MAXN SUPER, 2026-11-05)
      - 3 threads is slower again (~21ms/frame); frame age rises ~10ms (frames queue instead of being dropped)
      - remaining limit: MOG2 + morphology take ~17ms/frame at 1080p, right at the 16.7ms budget for 60fps
    * for this busy desk scene `--var-threshold 32 --min-area 50` roughly halves noise tracks; thresholds don't change
      per-frame cost (it's fixed by MOG2/morphology on the full frame, not by the number of candidates)
    * Orin storage: the NVMe (Kingston SNV3S) occasionally loses a completion interrupt -- reads hang until the kernel's
      30 s io_timeout recovers ("nvme nvme0: I/O tag ... timeout, completion polled" in journalctl -k; 2x on 2026-11-05,
      plus unlogged multi-second stalls). calibrate.py waits up to 60 s per frame. Common Jetson workarounds (not applied):
      kernel args nvme_core.default_ps_max_latency_us=0 (disable APST) and/or pcie_aspm=off
    * Orin power: run in MAXN SUPER (`sudo nvpmodel -m 2`); 25W mode was ~20% slower. Clocks still drop to ~0.9-1.2GHz
      between bursts under the default governor -- `sudo jetson_clocks` pins them (until reboot)
    * decodes with CPU jpegdec (~71 fps at 1080p); decode runs in parallel with the CV (~18 ms/frame), so it isn't the bottleneck
      - nvjpegdec rejects this camera's MJPEG: its frames have restart interval 1 (a marker every MCU), 4:2:2 chroma,
        and 139-2038 bytes of padding after EOI; trimming the padding doesn't help, libjpeg re-encoded frames decode fine
      - `nvv4l2decoder mjpeg=1 ! nvvidconv` decodes the frames as-is at ~133 fps, but gave no end-to-end gain and
        intermittently stalled (0 frames) in live capture -- revisit if decode ever becomes the bottleneck
    * measured at 1080p60: ~15-19ms/frame processing; live frame age ~42-46ms mean (capture to result)
  - don't put jpegparse in these pipelines -- it replaces capture timestamps with framerate-interpolated ones

### Phase 2: ????

** TBD **

## Open Questions
  - how do detect stationary targets -- e.g., hovering drone/hummingbird
    * either by an visual object classifier or a different type of sensor (e.g., lidar, thermal)

## Commercial Products
  - IdentiFlight
    * https://www.identiflight.com/
    * eight-camera array of wide FOV cameras and stereo PT high-res stereo tracking cameras
    * uses "NN technology" with high-res stereo cameras to determine distance and classify bird type, "in msecs"
      - 3 weeks to train for a new species
      - < 1% false negatives
      - < 2 sec classification [what about milliseconds claim above?]
    * determines 3D position, velocity, trajector, and predicted species of interest in realtime
      - 96% distance accuracy
      - 98% wingspan accuracy
      - 166 Mpix image data at > 5 imgs/s
    * detect and classify with 99% accuracy
    * up to 1.3km in distance
    * analyzes flight paths, shuts down wind farm if critical species detected
    * mounted on 6m high pole

## Other Projects

* Princeton undergrad project
  - used motion-based detection followed by SORT-derived tracker for stationary hummingbird videos
* ????

## References
  - PAN & TILT PLATFORM FOR A THERMAL VACUUM VIDEO CAMERA
    * https://ntrs.nasa.gov/api/citations/20070011748/downloads/20070011748.pdf

## Notes
