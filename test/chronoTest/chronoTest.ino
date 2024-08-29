/******************************************************************************
* 
* Airsoft Chronograph test program
*
* Uses HW ctr/tmr to get timestamps of start/stop events to measure speed
*
******************************************************************************/

#include <ArduinoLog.h>  // fatal, error, warning, notice, trace, verbose

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define START_IR_LED        5
#define START_IR_DETECTOR   2  // INT0
#define END_IR_LED          6
#define END_IR_DETECTOR     3  // INT1

#define BUTTON_PIN          4

#define SCREEN_ADDRESS      0x3C
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64


volatile uint16_t startTime = 0;
volatile uint16_t endTime = 0;
volatile uint16_t overflows = 0;
volatile bool measureDone = false;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


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

    // Configure Timer1
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
  
    TCCR1B |= (1 << CS10);   // normal mode with no prescaling (16MHz)
  
    TIMSK1 |= (1 << TOIE1);  // enable overflow interrupt
  
    // configure external interrupt pins
    //// TODO figure out if RISING or FALLING works better
    attachInterrupt(digitalPinToInterrupt(START_IR_DETECTOR), startMeasure, RISING);
    attachInterrupt(digitalPinToInterrupt(END_IR_DETECTOR), endMeasure, RISING);

    // digital start/stop inputs
    pinMode(START_IR_DETECTOR, INPUT);
    pinMode(END_IR_DETECTOR, INPUT);

    // IR LED pull-down enables
    pinMode(START_IR_LED, OUTPUT);
    pinMode(END_IR_LED, OUTPUT);
    digitalWrite(START_IR_LED, LOW);
    digitalWrite(END_IR_LED, LOW);

    // input button
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.flush();
};

void loop() {
    //// N.B. this loop runs at ????
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

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 0);
    if (digitalRead(BUTTON_PIN)) {
        display.println(F("ON"));
    } else {
        display.println(F("OFF"));
    }
    display.display();
};
