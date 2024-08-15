//// TODO figure out if I should ditch the PWM and just do 100%/0%

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

#define FEEDER_DUTY_CYCLE MAX_DUTY_CYCLE

#define TRIGGER_TIME    150  // 7 shots/sec = 143msec

#define PRIME_TIME      750  // time to prime the feeder line (@ current speed)


float freq;
float dutyCycle;
unsigned long lastTime;
unsigned int numShots;
bool primed;
unsigned long loopCnt = 0;

RP2040_PWM *PWM_Instance;
OnBoardLED *neoPix;

/*
constexpr int foo = 1;
constexpr void bar(int foo) {
  return;
}
*/

void checkInput() {
  if (Serial.available()) {
    byte cmd = Serial.peek();
    switch (cmd) {
    case 'C':
    case 'c':
      // try to unjam the feeder by going back and forth a bit
      PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
      digitalWrite(_SLEEP_PIN, LOW);
      delay(5);
      digitalWrite(_SLEEP_PIN, HIGH);

      digitalWrite(DIR_PIN, LOW);
      PWM_Instance->setPWM(PWM_PIN, freq, MAX_DUTY_CYCLE);
      delay(250);  //// TODO tune this value
      digitalWrite(DIR_PIN, HIGH);
      delay(250);  //// TODO tune this value
      PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
  
      Serial.println("Clear");
      break;
    case 'd':
      dutyCycle = Serial.parseFloat();
      if (dutyCycle < MIN_DUTY_CYCLE) {
        dutyCycle = 0.0;
      } else if (dutyCycle >= 100.0) {
        dutyCycle = MAX_DUTY_CYCLE;
      }
      Serial.print("Speed: "); Serial.println(dutyCycle);
      break;
    case 'n':
      numShots = Serial.parseInt();
      Serial.print(numShots);Serial.println(" Shots");
      break;
    case 'o':
      numShots = 0xFFFFFFFF;
      Serial.println("Continuous");
      break;
    case 's':
      numShots = 1;
      Serial.println("Single Shot");
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

  freq = 2000.0;
  dutyCycle = FEEDER_DUTY_CYCLE;
  lastTime = millis();
  numShots = 0;
  primed = false;

  // set up feeder motor controller
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(DIR_PIN, HIGH);
  pinMode(_SLEEP_PIN, OUTPUT);
  digitalWrite(_SLEEP_PIN, LOW);
  pinMode(CS_PIN, INPUT);  // ANALOG
  pinMode(_FAULT_PIN, INPUT); //, INPUT_PULLUP);
  digitalWrite(_SLEEP_PIN, HIGH);

  PWM_Instance = new RP2040_PWM(PWM_PIN, freq, 0.0);

  // set up trigger
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

  // set up Neo Pixel
  neoPix = new OnBoardLED(NEOPIXEL_POWER, PIN_NEOPIXEL);
  neoPix->setColor(BLACK);

  Serial.println("START");
}

//// FIXME make feeder on/off be independent of trigger pull times
//// FIXME deal with huge lag in reacting to inputs
void loop() {
  checkInput();

  if (numShots > 0) {
    if (primed == false) {
      PWM_Instance->setPWM(PWM_PIN, freq, FEEDER_DUTY_CYCLE);
      Serial.println(">");
      delay(PRIME_TIME);
      PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
      primed = true;
    }
    digitalWrite(TRIG_PIN, HIGH);  // pull trigger
    neoPix->setColor(GREEN);
    PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);  // start feeder
    delay(TRIGGER_TIME);
    numShots--;
    if (numShots > 0) {
      Serial.println("+");
    } else {
      Serial.println("-");
    }
  } else {
    PWM_Instance->setPWM(PWM_PIN, freq, 0.0);  // stop feeder
    digitalWrite(TRIG_PIN, LOW);  // release trigger
    neoPix->setColor(BLACK);
  }

  if (digitalRead(_FAULT_PIN) == 0) {
    if (dutyCycle > MIN_DUTY_CYCLE) {
      neoPix->setColor(RED);
      digitalWrite(_SLEEP_PIN, LOW);
      delay(5);
      digitalWrite(_SLEEP_PIN, HIGH);
    } else {
      neoPix->setColor(MAGENTA);
    }
  } else {
    neoPix->setColor(BLACK);
  }

  loopCnt++;
}