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
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define START_IR_LED        5
#define START_IR_DETECTOR   6
#define END_IR_LED          7
#define END_IR_DETECTOR     8

#define SCREEN_ADDRESS      0x3C
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64


volatile uint16_t startTime = 0;
volatile uint16_t endTime = 0;
volatile uint16_t overflows = 0;
volatile bool measureDone = false;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

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
    Log.notice(F("BEGIN"));

   if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Log.error(F("SSD1306 allocation failed"));
        for(;;); //// FIXME
    }
    display.clearDisplay();  // N.B. do this before display() to clear the logo
    display.display();
    display.invertDisplay(true);
    delay(1000);
    display.invertDisplay(false);
    Log.info(F("DONE"));

    blueLED = new OnBoardLED(LED_BUILTIN);
    blueLED->on();

    // Configure Timer1
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
  
    TCCR1B |= (1 << CS10);   // normal mode with no prescaling (16MHz)
  
    TIMSK1 |= (1 << TOIE1);  // enable overflow interrupt
  
    // configure external interrupt pins
//    attachInterrupt(digitalPinToInterrupt(START_PIN), startMeasure, RISING);
//    attachInterrupt(digitalPinToInterrupt(END_PIN), endMeasure, RISING);

    // digital start/stop inputs
    pinMode(START_IR_DETECTOR, INPUT_PULLUP);
    pinMode(END_IR_DETECTOR, INPUT_PULLUP);

    // IR LED pull-down enables
    pinMode(START_IR_LED, OUTPUT);
    pinMode(END_IR_LED, OUTPUT);
    digitalWrite(START_IR_LED, HIGH);
    digitalWrite(END_IR_LED, HIGH);

    blueLED->off();
    Serial.flush();
};

bool inVal = false;

void loop() {
    //// N.B. this loop runs at ~41KHz
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

    //// N.B. the next three lines take ~6.8usec
    bool i = digitalRead(START_IR_DETECTOR);

    if (i != inVal) {
        Serial.println(i);
        inVal = i;
    }
    if (inVal) {
        blueLED->on();
    } else {
        blueLED->off();
    }
};
