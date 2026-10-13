# Video-based Object Detection and Tracking

* Tracking-by-Detection (TbD)
  - two iterative processes: detect object from a frame, and associating them between frames

* Simple Online and Real-time Tracking (SORT)
  - uses Kalman filter to estimate the state (location and velocity) of an object
  - associates the detected bounding boxes with predicted bounding boxes
    * using location-based similarity (i.e., IoU)
    * occlusion causes errors that accumulate in the state estimation of objects
      - likely to fail to associate when an object reappears
  - DeepSORT introduces similarity based on the appearance feature of objects
  - StrongSort improves tracking performance by improving the image feature extraction and restoring trajectories using Gaussian smooth interpolation
  - both use appearance features to recover interrupted trajectories
    - hard to get appearance features in objects with small size (i.e., number of pixels)

* Object Detection
  - Slicing Aided Hyper Inference (SAHI)
    * divide image into small candidate regions and do detection on each of them
    * only small portion contain targets, so inefficient
  - Sound Source Localization has been used to filter object detection candidates
    * shows improved performance, but requires audio subsystem

* Association
  - common approach associate detected bounding boxes in a frame with bounding boxes predicted from previous frame
    * overlapping ratio of boxes (IoU) and appearance similarity is used
    * fast moving small objects do not create overlaps between adjacent frames
      - also hard to extract appearance features from small boxes

* Detection with Adaptive SAHI
  - don't scan the whole frame each time, but scan region around the predicted location
  - to detect new objects, sample detection candidate regions uniformly fron the entire image without replacement
    * discovers new objects and rediscover previously tracked and then occlued objects
  - feed candidate ROIs to YOLO v5 for detection

* Association based on DHSC
  - task is to assign consistent ID to each object by matching a previously tracked object with one detected in the current frame, according to similarity
  - two stage approach: first based on location and second based on appearance
    * small and fast objects associated using Distance-IoU (DIoU)
    * second stage matching only for objects that aren't matched in the first stage
      - cosine similarity between appearance features is used
  - save detection history of each tracked object
    * use weighted sum of similarities in track history, latest detection location, and predicted location
      - works even when predictions are unreliable
      
* MixFormer
  - Mixed Attention Module (MAM) for simultaneous feature extraction and target information integration
  - build trackers by stacking multipl MAMs and placing a localization head on top
  - two types of MixFormer trackers
    * a hierarchical tracker MixCvT
    * non-hierarchical tracker MixViT 
  - built asymetric attention scheme in MAM to reduce computational costs
  - MixFormer trackers SoTA on tracking benchmarks
    * LaSOT, TrackingNet, VOT2020, GOT-10k, OTB100, UAV123
  - https://github.com/MCG-NJU/MixFormer
  - modern trackers use multi-stage pipeline
    * backbone: extract generic features of tracking target and search area
      - feed target template to one CNN/Transformer model and search input to second CNN/Transformer -- both models share weights
    * integration module: allow exchange between tracking target and search area for subsequent target-aware localization
      - gets output from both models and generates output to Classification and Regression heads
      - key part that combines generic feature extraction and target-aware localization
      - typically use correlation-based operations and online learning algorithms
      - transformers provide attention-based integration which results in good tracking performance
        * typically use CNN for generic feature extraction
        * use attention for target information integration in the later parts of the pipeline
        * CNNs typically pre-trained on generic object detection and might neglect finer structure information used in tracking
        * also, CNNs don't do global modeling like Transformers do
    * task-specific heads: precisely localize the target and estimate its bounding box
  - [tracking template usually refers to pixels of a specific detected target, not a generic pre-defined image -- search area is ROI where the template is predicted to be now]
  - MixFormer takes template and search inputs and uses mixed attention-based backbone and single head
    * unifies visual feature extraction and target information integration
      - information interaction of target template and search area
    * hybrid interaction: both self-attention and cross-attention operations on tokes from the target template and search area
      - sel
  - 

* Strawman approach ideas
  - need calibrated geometry to drive the PTZ platform
    * a classical CV problem
  - learned single-object tracker on PTZ camera, and learned target detector and motion-assisted acquisition on the wide camera
  - object detector from the YOLO family of models
    * decide if a region has a target of interest
    * for wide-camera acquisition and periodic PTZ confirmation
  - siamese or transformer-based single-object tracker
    * template matching or correlation tracking of an initialized target
    * PTZ camera after acquiring a target
  - learned appearance features plus temporal association
    * matching detections using only position and bounding-box overlap
    * use for multiple wide-camera tracks, when crops contain enough detail
  - motion-assisted learned detection
    * full-frame inference on every frame
    * wide-camera, to preserve small-object detail while limited compute
  - [unclear if classical background subtraction is worse than a learned detector]
  - once a target is in the FOV of the PTZ camera, the problem become tracking (not detection)
    * i.e., given a target in this frame, (predict and) find it in the next frame
  - a learned single-object tracker compares the target's appearance with a search region in each new frame
  - not a bad idea to use ML for interpreting image data and explicit state estimation to predict motion
  - worth looking at
    * MixFormerV2
    * ByteTrack
  - proposed pipelines
    * wide camera
      - motion candidates & periodic wider scan
        * motion not required -- want to track hovering targets too
      - learned target detection on native resolution crops
      - multi-object tracking and target selection
      - predicted PTZ handoff
    * PTZ camera
      - detector acquires target
      - learned single-object tracker runs frequently
      - detector periodically verifies/corrects the track
      - angular prediction & PTZ controller
      - if lost, wide-camera reacquisition
  - can use multi-modal, fused, sensor data to aid detection -- not limited to vision-only
  - look into class-agnostic approach where the appearance, motion, and geometry are used to segment objects, without assigning semantic categories

* references
  - Video Class Agnostic Segmentation
    * https://msiam.github.io/vca/
  - MixFormer: End-to-End Tracking with Iterative Mixed Attention
    * https://arxiv.org/html/2302.02814v2
  - Princeton May25 video
    * CV pipeline
      - Motion-based object detection -> Multi-object tracking (MOT) based on SORT -> Automated analysis
        * SORT: Simple Online and Real-time Tracking algorithm
          - combines Kalman Filter for motion prediction and Hungarian algorithm for frame-to-frame data association based on Intersection over Union (IoU)
          - runs at ~260Hz on standard HW
          - relies on geometric bounding box overlap -- struggles with occlusions
        * DeepSORT
          - Simple Online and Realtime Tracking with a Deep Association Metric
            * https://arxiv.org/abs/1703.07402
          - integrates deep association metric learn from people re-identification dataset to maintain identities through occlusions by matching visual appearance features
            * might only work for people not arbitrary objects
      - claims to outperform YOLO
  - Tracking Small Birds by Detection Candidate Region Filtering and Detection History-aware Association
    * https://arxiv.org/html/2405.17323v1
  - Evaluation of trackers for Pan-Tilt-Zoom Scenarios
    * https://arxiv.org/html/1711.04260v1
  - An Active Multi-Object Ultrafast Tracking System with CNN-Based Hybrid Object Detection
    * https://pmc.ncbi.nlm.nih.gov/articles/PMC10145589/
  - Hybrid Multi-camera Visual Servoing to Moving Target
    * https://arxiv.org/html/1803.02285v2
  - Keeping Multiple Objects in the Field of View of a Single PTZ Camera
    * https://skoge.folk.ntnu.no/prost/proceedings/acc09/data/papers/0035.pdf
  - Visual Servoing of a Moving Target by an Unmanned Aerial Vehicle
    * https://pmc.ncbi.nlm.nih.gov/articles/PMC8434602/
  - Tracking Small Birds by Detection Candidate Region Filtering and Detection History-aware Association
    * https://ui.adsabs.harvard.edu/abs/2024arXiv240517323L/abstract
  - ByteTrack: Multi-Object Tracking by Associating Every Detection Box
    * https://scispace.com/pdf/bytetrack-multi-object-tracking-by-associating-every-22bdtggc.pdf
  - DESIGN AND APPLICATION OF PAN AND TILT SERVO GIMBALS IN POINTING, ACQUISITION, AND TRACKING
    * https://api.drum.lib.umd.edu/server/api/core/bitstreams/a20e2780-5d27-4d4b-b675-0558d0fafae0/content
  - Visual Servoing Platform
    * https://visp-doc.inria.fr/doxygen/visp-daily/tutorial-ibvs.html
    * https://github.com/lagadic/visp/wiki
  - Predictive tracking of an object by a pan–tilt camera of a robot
    * https://link.springer.com/article/10.1007/s11071-023-08295-z



