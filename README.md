# AutoTurret
An automated turret

## Notes

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
    * Sleep (/SLEEP): starts up in sleep mode, logic high input brings out of sleep
    * Motor Power (VIN): 4.5-37 V motor voltage input, reverse-voltage protected [use this]
    * Motor Voltage (VM): motor voltage after reverse-voltage protection
      - can use this to supply reverse-voltage protected voltage
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
    * OUT1/OUT2: goes to motor
  - 

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


* Links
  - https://www.pololu.com/product/4036
  - https://www.pololu.com/product/4037
