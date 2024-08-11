#include "RP2040_PWM.h"

#include <OnBoardLED.h>


#define PWM_PIN     D3    // EN
#define DIR_PIN     D2    // PH
#define _SLEEP_PIN  D1    // _SLEEP
#define CS_PIN      A0    // CS
#define _FAULT_PIN  D10   // _FAULT
#define TRIG_PIN    D9

#define MIN_DUTY_CYCLE  11.0
#define MAX_DUTY_CYCLE  99.9
#define MIN_FREQ        500

#define TRIGGER_TIME    150  // 7 shots/sec = 143msec


RP2040_PWM *PWM_Instance;
OnBoardLED *neoPix;

float freq = 2000.0;
float dutyCycle = 0.0;
unsigned long lastTime = millis();
unsigned long loopCnt = 0;
float currentSense = 0;
unsigned int numShots = 0;


void checkInput() {
  if (Serial.available()) {
    byte cmd = Serial.peek();
    switch (cmd) {
    case 's':
      numShots = 1;
      Serial.println("Single Shot");
      break;
    case 'n':
      numShots = Serial.parseInt();
      Serial.print(numShots);Serial.println(" Shots");
      break;
    case 'o':
      numShots = 0xFFFFFFFF;
      Serial.println("Continuous");
      break;
    default:
      numShots = 0;
      Serial.println("Stop Shooting");
      break;
    }

    // flush the input
    while (Serial.available()) {
      Serial.read();
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; };
  Serial.println("BEGIN");

  // set up feeder motor controller
  pinMode(DIR_PIN, OUTPUT);
  pinMode(_SLEEP_PIN, OUTPUT);
  pinMode(CS_PIN, INPUT);  // ANALOG
  pinMode(_FAULT_PIN, INPUT_PULLUP);
  digitalWrite(_SLEEP_PIN, LOW);
  digitalWrite(DIR_PIN, HIGH);

  PWM_Instance = new RP2040_PWM(PWM_PIN, freq, dutyCycle);
  PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);

  // set up trigger
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

  // set up Neo Pixel
  neoPix = new OnBoardLED(NEOPIXEL_POWER, PIN_NEOPIXEL);
  neoPix->setColor(BLACK);

  Serial.println("START");
}

void loop() {
  checkInput();

  if (numShots > 0) {
    //// TODO make sure that the feeder motor is on
    digitalWrite(TRIG_PIN, HIGH);
    delay(TRIGGER_TIME);
    numShots--;
  } else {
    //// TODO turn off feeder moter
    digitalWrite(TRIG_PIN, LOW);
  }

  if (dutyCycle > MIN_DUTY_CYCLE) {
    if (digitalRead(_FAULT_PIN) == 0) {
      neoPix->setColor(RED);
    } else {
      neoPix->setColor(GREEN);
    }
  } else {
    neoPix->setColor(BLACK);
  }

  loopCnt++;
}
