/******************************************************************************
* 
* Airsoft Chronograph test program
*
* Uses HW ctr/tmr to get timestamps of start/stop events to measure speed
*
* If reset button pressed at power on, enter Imperial Units mode, else run in
*  Metric Units mode.
*
******************************************************************************/

#include <ArduinoLog.h>  // fatal, error, warning, notice, trace, verbose

#include <stdint.h>

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

#define SENSOR_DISTANCE     0.04  // meters between start and end sensors
#define GRAVITY             9.8   // m/s^2

#define METERS_TO_FEET(m)   (m * 3.28084)


volatile uint16_t startTime;
volatile uint16_t endTime;
volatile uint16_t overflows;
volatile bool measureDone;

float elapsedTime;
uint32_t count;
unsigned long totalTime;
uint32_t minTime;
uint32_t avgTime;
uint32_t maxTime;

float velocity;

bool metric;

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

void initVars() {
    elapsedTime = 0.0;
    count = 0;
    totalTime = 0;
    minTime = UINT32_MAX;
    avgTime = 0;
    maxTime = 0;

    startTime = 0;
    endTime = 0;
    overflows = 0;

    velocity = 0;

    measureDone = false;
}

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

    initVars();

    metric = (digitalRead(BUTTON_PIN) ? true : false);

    Serial.flush();
};

void loop() {
    float minVelocity;
    float avgVelocity;
    float maxVelocity;

    if (!digitalRead(BUTTON_PIN)) {
        initVars();
        Log.info(F("Reset"));
    }

    if (measureDone) {
        uint32_t elapsed = ((uint32_t)endTime + ((uint32_t)overflows * 65536) -
                            (uint32_t)startTime);
        elapsedTime = (elapsed / 16.0);  // microseconds

        count++;
        totalTime += elapsedTime;
        minTime = (elapsedTime < minTime) ? elapsedTime : minTime;
        avgTime = (totalTime / count);
        maxTime = (elapsedTime > maxTime) ? elapsedTime : maxTime;

        velocity = ((SENSOR_DISTANCE * 1000000.0) / elapsedTime);
        minVelocity = ((SENSOR_DISTANCE * 1000000.0) / maxTime);
        avgVelocity = ((SENSOR_DISTANCE * 1000000.0) / avgTime);
        maxVelocity = ((SENSOR_DISTANCE * 1000000.0) / minTime);
        if (!metric) {
            velocity = METERS_TO_FEET(velocity);
            minVelocity = METERS_TO_FEET(minVelocity);
            avgVelocity = METERS_TO_FEET(avgVelocity);
            maxVelocity = METERS_TO_FEET(maxVelocity);
        }

        measureDone = false;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // N.B. size=1: 5x7 chars, 21x chars/line, 8x lines;
    // N.B. size=2: 10x16 chars, 11x chars/line, x4 lines (no line spaces);
    // N.B. can enable special chars with 'display.cp437(true);'
    // first line: last time, count
    char charBuf[21];
    char veloStr[6];  // xxx.xx m/s (or fps)

    display.setTextSize(2);
    display.setCursor(0, 0);

    dtostrf(velocity, 4, 1, veloStr);
    sprintf(charBuf, "%c%s #%d", (metric ? 'M' : 'I'), veloStr, count);
    display.println(charBuf);
    if (count) {
        display.setCursor(0, 17);

        dtostrf(minVelocity, 4, 2, veloStr);
        sprintf(charBuf, "min: %s", veloStr);
        display.println(charBuf);

        dtostrf(avgVelocity, 4, 2, veloStr);
        sprintf(charBuf, "avg: %s", veloStr);
        display.println(charBuf);

        dtostrf(maxVelocity, 4, 2, veloStr);
        sprintf(charBuf, "max: %s", veloStr);
        display.println(charBuf);
    }
    display.display();
};
