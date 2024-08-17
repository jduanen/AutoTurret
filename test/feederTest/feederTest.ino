#include "RP2040_PWM.h"

#include <OnBoardLED.h>


#define PWM_PIN     D3    // EN
#define DIR_PIN     D2    // PH
#define _SLEEP_PIN  D1    // _SLEEP
#define CS_PIN      A0    // CS
#define _FAULT_PIN  D10   // _FAULT

#define MIN_DUTY_CYCLE  11.0
#define MAX_DUTY_CYCLE  99.9
#define MIN_FREQ        500


RP2040_PWM *PWM_Instance;
OnBoardLED *neoPix;

float freq = 2000;
float dutyCycle = 0.0;
unsigned long lastTime = millis();
unsigned long loopCnt = 0;
float currentSense = 0;


// try to unjam the feeder by going back and forth a bit
void clearFeeder() {
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
}

void checkInput() {
  byte cmd;
  uint32_t duration;

  if (Serial.available()) {
    cmd = Serial.peek();
    switch (cmd) {
    case 'C':
    case 'c':
      Serial.println("Clear");
      clearFeeder();
      break;
    case 'S':
      // start max speed
      dutyCycle = MAX_DUTY_CYCLE;
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      Serial.println("Start");
      break;
    case 'f':
      // set frequency
      freq = Serial.parseFloat();
      freq = (freq < MIN_FREQ) ? MIN_FREQ : freq;
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      Serial.print("Freq: "); Serial.println(freq);
      break;

      dutyCycle = Serial.parseFloat();
      if (dutyCycle < MIN_DUTY_CYCLE) {
        dutyCycle = 0.0;
      } else if (dutyCycle >= 100.0) {
        dutyCycle = MAX_DUTY_CYCLE;
      }
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      Serial.print("Speed: "); Serial.println(dutyCycle);
      break;
    case 'x':
      dutyCycle = Serial.parseFloat();
      if (dutyCycle < MIN_DUTY_CYCLE) {
        dutyCycle = 0.0;
      } else if (dutyCycle >= 100.0) {
        dutyCycle = MAX_DUTY_CYCLE;
      }
      duration = Serial.parseInt();

      Serial.print("Duty Cycle: "); Serial.print(dutyCycle);
      Serial.print(", Duration: "); Serial.print(duration);
      Serial.println(" (msec)");
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      delay(duration);
      PWM_Instance->setPWM(PWM_PIN, freq, 0);
      break;
    default:
      // stop
      dutyCycle = 0.0;
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      Serial.println("Stop");
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

  pinMode(DIR_PIN, OUTPUT);
  pinMode(_SLEEP_PIN, OUTPUT);
  pinMode(CS_PIN, INPUT);  // ANALOG
  pinMode(_FAULT_PIN, INPUT_PULLUP);

  digitalWrite(_SLEEP_PIN, LOW);
  digitalWrite(DIR_PIN, HIGH);

  PWM_Instance = new RP2040_PWM(PWM_PIN, freq, dutyCycle);
  PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
  Serial.print("Initial Duty Cycle: ");Serial.println(dutyCycle);

  neoPix = new OnBoardLED(NEOPIXEL_POWER, PIN_NEOPIXEL);
  neoPix->setColor(BLACK);

  digitalWrite(_SLEEP_PIN, HIGH);
  Serial.println("START");
}

void loop() {
  checkInput();

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
