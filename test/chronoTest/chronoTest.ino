/******************************************************************************
* 
* Airsoft Chronograph test program
*
* Uses HW ctr/tmr to get timestamps of start/stop events to measure speed
* 
* ATmega328P
*   - Timers:
*       * Timer/Counter 0: 8-bit
*       * Timer/Counter 1: 16-bit (with Input Capture capability)
*       * Timer/Counter 2: 8-bit
* ATmega2560
*   - Timers:
*       * Timer/Counter 0: 8-bit
*       * Timer/Counter 1: 16-bit (with Input Capture capability)
*       * Timer/Counter 2: 8-bit
*       * Timer/Counter 3: 16-bit (with Input Capture capability)
*       * Timer/Counter 4: 16-bit (with Input Capture capability)
*       * Timer/Counter 5: 16-bit (with Input Capture capability)
* ATtiny84
*   - Timers:
*       * Timer/Counter 0: 8-bit
*       * Timer/Counter 1: 16-bit (with Input Capture capability)
*
******************************************************************************/

#include <ArduinoLog.h>  // fatal, error, warning, notice, trace, verbose

#include <OnBoardLED.h>


#define START_PIN   2
#define END_PIN     3


volatile uint16_t startTime = 0;
volatile uint16_t endTime = 0;
volatile uint16_t overflows = 0;
volatile bool measureDone = false;

OnBoardLED *blueLED;


ISR(TIMER1_OVF_vect) {
    overflows++;
};

void startMeasure() {
  TCNT1 = 0;  // reset timer
  overflows = 0;
  startTime = TCNT1;
};

void endMeasure() {
  endTime = TCNT1;
  measureDone = true;
};

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; };
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);  // Error, Info, Warn, Debug, Verbose
    Log.notice("BEGIN");Serial.flush();

    blueLED = new OnBoardLED(LED_BUILTIN);
    blueLED->on();

    // Configure Timer1
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
  
    TCCR1B |= (1 << CS10);   // normal mode with no prescaling (16MHz)
  
    TIMSK1 |= (1 << TOIE1);  // enable overflow interrupt
  
    // configure external interrupt pins
    attachInterrupt(digitalPinToInterrupt(START_PIN), startMeasure, RISING);
    attachInterrupt(digitalPinToInterrupt(END_PIN), endMeasure, RISING);

    blueLED->off();
};

void loop() {
    blueLED->on();
    if (measureDone) {
        uint32_t totalTime = ((uint32_t)endTime +
                              ((uint32_t)overflows * 65536) - 
                              (uint32_t)startTime);
        float timeDiff = (totalTime / 16.0);  // microseconds
    
        Serial.print("Time diff: ");
        Serial.print(timeDiff);
        Serial.println(" usec");
    
        measureDone = false;
    }
    blueLED->off();
};
