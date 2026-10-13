# AutoTurret

An automated turret for detecting and tracking objects.

## Requirements

- Detection
  * multi-modal sensors (visual, acoustic, thermal, RF, Lidar, etc.)
  * ????
- Tracking
  * ????
- Video Streaming
  * all payloads include video sensor
    - variable focus? digital/analog zoom?
  * low latency video capture for local processing (for local vision tasks)
  * video streamed over WiFi with low latency
- Payload
  * ????
- Telemetry
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


## Features

* Servo-Driven Pan and Tilt Platform
  - ** TBD **

## Notes

* Example targets and their key characteristics
  - Drones
    * emit sounds (motor/jet/prop noise)
    * may emit radiation (RF links)
    * assume that they are both larger, slower, and lower accleration than hummingbirds
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
 
* Vision-servo tracking systems
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
    * biggest problem is latency -- must do detection, motion predition, and move before object leaves the FOV
    * works better for following an acquired target, as opposed to continuously detecting all targets in the FOV
    * target's angular speed relative to the camera is important (not just linear speed)
    *  w ~ v_p / r
      - w: angular speed
      - v_p: velocity perpendicular to the line of sight
      - r: range
    * e.g., 10 m/s target speed at 10 m from camera -- 57 deg/s
      - gets more demanding to track the closer the object is from the camera
      - at the same time it gets harder to detect the futher the object is from the camera
    * e.g., 57 deg/s angular speed, with total delay of 100ms, object moves 5.7 deg in that time
      - if 10 deg HFOV, then object will leave the frame before the system can respond
    * prediction helps, but is defeated by rapid changes in direction
      - prediction requires both speed and robustness
      - prediction has to account for target motion, camera motion, and delays
      - predict where the target will be when the mount arrives
      - basic angular prediction
        * thetaHat(t + T) = theta(t) + thetaDot(t)T
          - T includes frame age, inference time, comm, and mount response
        * e.g., target moving at 60deg/s moves 6deg in 100msec, so acquisition FOV must accomodate both this motion and prediction uncertainty
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
      - estimaged target angular velocity as feed-forward
      - prediction over measured feedback delay
      - rate and acceleration limis
      - modest central deadband to reduce jitter
    * estimate target motion in a camera-motion-compensated coordinate system
      - otherwise, the mount's movement will look like target movement
  - handle target acquisition and loss explicitly
    * state machine: Search -> Aquire -> Track -> Lost -> Reacquire/Search
    * on loss, breifly search near the predicted direction, then widen view (if possible, else scan)
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
    * measure pan/tilt zero offets and actual angle response
    * calibrate focused camera at the chosen focal length (if PZT camera is fixed-focus)
    * check pointing accuracy with stationary targets -- both near and far ones
  - for a calibrated rectilinear camera, an undistorted pixel can be converted into a viewing ray
    * q -> K^-1 [u v 1]^T
    * rotate that ray into the mount's coordinate system, then derive the desired P&T angles
    * for a fisheye lens, use its calibrated project model instead
  - start with PZT camera fairly wide (e.g., 20deg) then narrow zoom as tracking improves
  - doesn't provide range information
    * can get it by using stereo PZT cameras or another sensor (lidar, RF, etc.)

* Hybrid SW approach
  - classical motion processing and geometric control for speed
  - learned detection for target recognition
  - temporal tracking to connect observations

* Target Detection and Tracking
  - Detection: locates and object in an image
  - Classification: assigns a category to a detected object
  - Tracking: links observations over time and estimates future motion; assigns IDs to tracked objects
  - Recognition: distinguishes categories, and in the limit, individual identities

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
    * tiny, blurred, similar-looking targest offer weak identity cues
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
    * motion dtection plus classical association
    * learned detection plus classical association
    * motion-guided learned detection plus classical association
    * the best of the above approaches, with a faster local tracker between detections
  - measure
    * target detection recall: how often are visible targets found
    * false postives per minute
    * track fragmentation and ID switches
    * localization error: imprtant for pointing the PZT camera
    * frame age when the control command is issued
    * acquistion time and percentage of time the target remains focused in view
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

* PZT platform
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
    * an ideal 90deg acquistion takes about 400ms, including acceleration and breaking times
  - real motion takes even longer when jerk limits, structural vibration, controller delay, and settling are taken into account
  - most meaningful spec is loaded speed-torque capability with actual payload intertia
  - calculate torque separately for each axis
  - pan axis doesn't create a direct driving torque
    * but it has to move the whole assembly, so the inertia will be greater than the tilt axis'
  - adding weight to balance payloads results in higher intertia
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
  - possible motor choices
    * Teknic CPM-SDSK-2310S-ELN
      - NEMA 23, with Enhanced Resolution
      - Peak torque (at 75 VDC): 1.6 N·m (226.6 oz-in)
      - Continuous RMS torque (at 75 VDC): 0.3 N·m (42.48 oz-in)
      - Maximum speed (at 75 VDC): 4,000 rpm
      - Achievable positioning resolution: Approximately 0.057°
      - Repeatability: 0.03°
      - Rotor inertia: 0.1 kg·cm²
      - Control interface: Step/direction or quadrature A/B
      - Weight: 600 g [more than the payload/camera]
￼     - use this motor with 10:1--15:1 reduction (15:1)
    * Teknic CMP-SCSK-2310S-ELNB <<<=== good choice
      - NEMA 23, with Enhanced Resolution and Basic firmware
      - SW-controlled step and dir
      - Peak Torque at speed at 75VDC: 64 oz-in, 4000 RPM
      - Continuous Torque at 75VDC: 13 oz-in
      - Max Speed at 75VDC: 4000 RPM
      - Resolution: 0.057 deg
      - Repeatability: 0.03 deg
      - Rotor Inertia: 0.1 kg-cm^2
      - Weight: 0.6 kg
      - Max Radial Load: 111.2 N
      - Max Thrust Load: 22.2 N
      - Price: $368
    * Teknic CMP-SCSK-2310P-ELNB
      - NEMA 23, with Enhanced Resolution and Basic firmware
      - SW-controlled step and dir
      - Peak Torque at speed at 75VDC: 112 oz-in, 4000 RPM
      - Continuous Torque at 75VDC: 22.5 oz-in
      - Max Speed at 75VDC: 4000 RPM
      - Resolution: 0.057 deg
      - Repeatability: 0.03 deg
      - Rotor Inertia: 0.1 kg-cm^2
      - Weight: 0.6 kg
      - Max Radial Load: 111.2 N
      - Max Thrust Load: 22.2 N
      - Price: $368
    * Teknic CPM-ECSK-1730P-ELNB
      - NEMA 17, with Enhanced Resolution and Basic firmware
      - EtherCAT interface
      - Peak Torque at speed at 75VDC: 34 oz-in, 1000 RPM
      - Continuous Torque at 75VDC: 27.5 oz-in
      - Max Speed at 75VDC: 1000 RPM
      - Resolution: 0.007 deg
      - Repeatability: 0.007 deg
      - Rotor Inertia: 0.1 kg-cm^2
      - Weight: 0.1 kg
      - Max Radial Load: 111.2 N
      - Max Thrust Load: 25.7 N
      - Price: $435
    * Teknic CPM-ECHP-1730P-ELNB
      - NEMA 17, with Enhanced Resolution and Basic firmware
      - EtherCAT
      - Peak Torque at speed at 75VDC: 43 oz-in, 3000 RPM
      - Continuous Torque at 75VDC: 34.1 oz-in
      - Max Speed at 75VDC: 3000 RPM
      - Resolution: 0.007 deg
      - Repeatability: 0.007 deg
      - Rotor Inertia: 0.1 kg-cm^2
      - Weight: 0.1 kg
      - Max Radial Load: 111.2 N
      - Max Thrust Load: 25.7 N
      - Price: $499

* Teknic ClearPath products "stepper killer"
  - servo for the price of stepper, with 2-3x more power
  - MC: digital I/O motion control
  - SD: control with Step & Direction
    * Enhanced SDSK: step/direction interface, with enhanced resolution
  - SC: SW controlled (Free C++ library)
    * Enhanced Firmware: provides more features over Basic Firmware
      - g-Stop anti-vibration move profiles
      - Triggered moves
      - Head-tail moves
      - Asymmetric moves
      - Trigger events at user-defined positions
      - Pos/Neg torque limiting
      - Conditional torque limiting
      - Attention generation
      - Position capture
  - IP: Ethernet/IP controlled, needs master controller
  - EC: EtherCAT controlled, needs master controller
    * Enhanced ClearPath-EC, EtherCAT interface, with enhanced resolution
    * EtherCAT, cyclic synchronous control modes
      - min EtherCAT cycle time of 250usec -- provides position/velocity/torque at that rate
    * requires EtherCAT master and SW integration
    * NEMA-17 version might be smaller/lighter and easier to integrate
    * 51,200 counts/rev for Enhanced EC -- 0.00703 deg/count
      - closer to direct drive requirements
    * EtherSAT controllers
      - best choice is RK3588 board with PREEMPT_RT
        * RTOnBoot or IgH 1.6

* computing requirements
  - on-board vs. remote
  - control-loop processing has to be local for latency reasons
  - can remotely provide higher-level supervision and guidance (longer time constants)
  - can use RasPi 4/5, NVDA Tegra/Orin Nano/etc.
  - ????

* Open questions
  - how do detect stationary targets -- e.g., hovering drone/hummingbird
    * either by an visual object classifier or a different type of sensor (e.g., lidar, thermal)

* commercial products
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

* References
  - PAN & TILT PLATFORM FOR A THERMAL VACUUM VIDEO CAMERA
    * https://ntrs.nasa.gov/api/citations/20070011748/downloads/20070011748.pdf
