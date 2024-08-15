# AutoTurret
An automated turret

## Features

* Servo-Driven Pan and Tilt Platform
  - ** TBD **

* Airsoft Armaments
  - Automatic Electric Gun (AEG)
    * for initial testing
    * ** TBD **
  - High-Pressure Air (HPA)
    * ** TBD **
  - High-Speed Feeder
    * requirements
      - <= 3000 pellets/min (50 pellets/sec)
      - no jamming over full capacity of pellets
      - sufficient capacity to match air supply
      - operates in all orientations (i.e., don't rely on gravity feed)
      - flexible feeder line
      - effective range <15 meters
      - ? J, ? fps
    * design features
      - ?
    * feeder data
      - 20V@?A: 44.5 p/s
        * max rate (24V@?A): 55 p/s
      - current limiting resistor for DRV8876
        * ? ohms: ????

* Sensors
  - Presence Detection
    * 24GHz radar
      - ** TBD **
    * PIR
      - ** TBD **
  - Vision-Based Detection
    * ** TBD **
  - Vision-Based Targeting
    * ** TBD **
  - Video Streaming
    * ** TBD **
  - Telemetry
    * ** TBD **
  - Airsoft Sensors
    * Chronograph
      - add pair of IR emitter/receiver pairs to tube added to end of barrel
        * make tube slightly larger than bore size
        * place emitter and receiver on opposite sides of the tube
        * separate emitter/receiver pairs by 50mm or 100mm
          - depends on achievable accuracy
        * ?
      - ?
    * Pellet Supply
      - <reference to model source>
        * <describe the mods I made>
        * <add cad files>
      - <flexible feeder tube>
      - 
    * Air Supply
      - ** TBD **
    * Orientation
      - absolute, repeatable, accurate (IMU, magnetometer)
      - ** TBD **

## Notes

* Test Environment
  - start with cheap AEG engine
    * remove from original plastic
    * design and print a new enclosure
  - create a test target
    * use 70mm cardboard shipping tube
    * add high-density foam with 45 degree face at far end
    * create a "plug" around the AEG barrel with high-density foam

* MOSFET AEG Trigger Circuit
  - put decoupling cap and back-EMF diode on AEG's DC motor
    * put them right on the motor terminals (avoid lead inductance)
  - MOSFET circuit
    * DC Motor connects between voltage source and Drain
    * Source connects to ground
    * pull-up resistor from Gate to motor voltage source to ensure it's turned on when trigger signal is high
    * maybe use the 20V supply voltage to run the AEG motor
      - if so, need to recalibrate firing rate
  - Motor supply must support high in-rush and high sustained current
  - use IRF540 because it has a lower Vth than the IRF520
  - 3.3V still isn't enough to fully saturate the IRF540
    * need to drive Gate to higher voltage (around 4.6V)
    * have to drive FET into saturation/low R
  - use 2N3904 to act as a level shifter from 3.3V input from controller to >5V output to Gate
  - RP2040 puts all GPIOs into HiZ state while loading (booting?)
    * need to make sure that the trigger doesn't activate during load/reboot
    * use pull-down resistor on input to level shifting transistor
  - 

* Chronograph
  - use ATmega2560, ATmega32U4, ATmega1284P, 

* Pololu DRV8876 board
  - one motor channel
  - 1.3 A max continuous current (no heat sink)
  - adjustable current limiting
    * 2 A default
    * adjustable by pot/resistor across empty pads in bottom left of board
  - $6.95 (1 unit)
  - protection
    * under-voltage lockout
    * over-current and over-temperature
    * reverse-voltage
  - modes:
    * Phase/Enable (PH/EN): ?
    * PWM (IN/IN): ?
    * Independent half-bridge control: ?
  - logic voltage: 1.8-5.5 V
  - PWM input: <=100 kHz
  - pins
    * Current Sense (CS): analog output, 2500 mV/A
      - really noisy and have to either avg/filter or sample fast and avg
      --> not using this
    * Sleep (/SLEEP)
      - active low, put device to low-power sleep
      - starts up in sleep mode, logic high input brings out of sleep
      - takes Tsleep (1ms) to take effect
      - reads strapping pin states when in sleep mode
        * takes effect when coming out of sleep mode (rising edge of /SLEEP)
    * Motor Power (VIN): 4.5-37 V motor voltage input, reverse-voltage protected [use this]
    * Motor Voltage (VM): motor voltage after reverse-voltage protection
      - can use this to supply reverse-voltage protected voltage
      - need put decoupling caps on this
        --> already on the Pololu board
    * Operating Mode (PMODE):
      - 0/low: Phase/Enable mode, EN=PWM
        * PH=1: forward/brake at PWM% speed
        * PH=0: reverse/brake at PWM% speed
        * PH=PWM, EN=1/high
          - locked-antiphase mode
            * 0%: full speed in one direction
            * 50%: stop
            * 100%: full speed in the other direction
      - can do more advanced operations, but I'm using this mode
        * allows me to jog the feeder back and forth to help clear jams
    * OUT1/OUT2: goes to motor
    * Current Regulation and Protection Mode (IMODE)
      - four possible modes, based on state of pin
        * GND: fixed off-time, automatic retry, overcurrent only
        * 20K to GND: cycle-by-cycle, automatic retry, both current chopping and overcurrent
        * 62K to GND: cycle-by-cycle, outputs latched off, both current chopping and overcurrent
        * HiZ: fixed off-time, outputs latched off, overcurrent only
      - Overcurrent
        * disables outputs and enters a brake state for Toff time
          - in brake state both low-side MOSFETs are turned on
        * if EN or PH pins change during Toff:
          - exits Break state, resets and follows inputs again
        * Toff: 25 usec
        * Tdelay: 1.6 usec
      - Fixed Off-Time Current Chopping
        * can use 100% duty cycle in this mode
      - Cycle-by-Cycle Current Chopping
        * can't use 100% duty cycle in this mode
          - needs a new control input edge to exist Break state
        * pulls /FAULT pin low when chopping
      - Auto-Retry mode
        * if over-current for more than Tocp (3 usec)
          - disables MOSFETs and drives /FAULT low for Tretry (2 msec)
          - resets based on control inputs
            * resumes normal operation, repeats if still over-current
      - Latched-Off mode
        * disables MOSFETs, drives /FAULT low until reset
          - resets by /SLEEP pin or removing power (via VM)
    * Fault (/FAULT)
      - active low (open-drain), indicates over-current/-temperature
      - needs pull-up resistor (or PULLUP pin mode in Arduino)
        * 10K pullup works better than internal pull-up mode [don't know why]
      - just an indication, doesn't change operation
      - fault indicates current chopping if driving motor forward/reverse
        * otherwise its a device fault
        * if /FAULT=LOW and control inputs selecting hi-Z or slow-decay
          - then it's a device fault
  - need add a heat-sink as it gets hot
  - Tsleep: time to sleep from when nSleep=LOW

* Xiao RP2040
  - A0 values connected to CS (avg over 10 samples)
    * strong 2 KHz noise at slow speeds, to 1 KHz at full speed
      - this is the PWM frequency set in the test code
    * no load signal at different speeds (avg, voltage)
      - grounded: 0.3-0.4, n/a
      - 0%:   .7-.9, (Vmax=0mV,   Vmin=0mV,   Vavg=0mV)
      - 10%:  1-6,   (Vmax=222mV, Vmin=35mV,  Vavg=111mV)
      - 20%:  1-8,   (Vmax=275mV, Vmin=15mV,  Vavg=111mV)
      - 30%:  1-10,  (Vmax=315mV, Vmin=19mV,  Vavg=127mV)
      - 40%:  1-10,  (Vmax=356mV, Vmin=18mV,  Vavg=140mV)
      - 50%:  1-11,  (Vmax=375mV, Vmin=12mV,  Vavg=147mV)
      - 60%:  1-11,  (Vmax=371mV, Vmin=3mV,   Vavg=150mV)
      - 70%:  1-11,  (Vmax=356mV, Vmin=3mV,   Vavg=147mV)
      - 80%:  1-11,  (Vmax=318mV, Vmin=9mV,   Vavg=145mV)
      - 90%:  2-6,   (Vmax=272mV, Vmin=57mV,  Vavg=149mV)
      - 100%: 4-6,   (Vmax=208mV, Vmin=109mV, Vavg=150mV)
    * loaded: Vavg goes up significantly
    * CS reflects the PWM frequency
      - need to sample fast enough (>nyquist) to get good avg in SW
      - created a LPF with R=100K and C=.1uF to smooth the noise
    * 

* Airsoft data
  - Kinetic Energy: 1/2mv^2
      - 0.2g  6mm pellet at 400 FPS (122 m/s): 1.5 J
      - 0.2g  6mm pellet at 500 FPS (152 m/s): 2.3 J
      - 0.2g  6mm pellet at 600 FPS (183 m/s): 3.3 J
      - 0.25g 6mm pellet at 360 FPS (110 m/s): 1.5 J
      - 0.35g 6mm pellet at 450 FPS (137 m/s): 3.3 J
      - 0.4g  6mm pellet at 600 FPS (183 m/s): 6.7 J
  - Safety
    * energy levels
      - safe:   <2 J
      - strong: >2 J && <4 J
      - unsafe: >4 J
    * values in different regulations
      - 1.35 J: penetrating wound
      - 3.0-4.0 J: penetrating/leathal wound
      - 2.0-3.0 J: penetration level
    * human skin penetration (> 1/2 diameter of pellet)
      - 162.1e^(-0.38sqrt(m))
      - e.g., 0.2 g @ 136.7 m/s (448 FPS) -- possible with AEG
  - Velocity Range
    * 180-740 fps (55-226 m/s)
  - Pellet Weights
    * 0.2-0.49 g
  - Rate of Fire
    * HPA: 35-60 pellets/sec
    * AEG: 100-1500 pellets/min
  - Barrel Length
    * 221-430 mm
  - AEG Piston-Barrel Volume Ratio
    * 2:1
  - Typical AEG
    * Velocity: 120 m/s (~400 fps)
    * Energy @ Distance
      - 1.4 J @  0 m
      -   1 J @  5 m
      - 0.6 J @ 10 m
      - 0.3 J @ 20 m
  - Ballistics
    * velocity drops exponentially with distance (due to the quadratic drag)
    * simple equation describes ideal (no hop-up, no wind) distance for given pellet diameter, weight, and speed
      - e.g., 6mm 0.2g 120m/s 1.8m height: hits ground at 34m

* Links
  - https://www.pololu.com/product/4036
  - https://www.pololu.com/product/4037
  - https://en.wikipedia.org/wiki/Airsoft_pellets
  - 
