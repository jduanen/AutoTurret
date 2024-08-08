#include "RP2040_PWM.h"

#include <OnBoardLED.h>


#define PWM_PIN     D3    // EN
#define DIR_PIN     D2    // PH
#define _SLEEP_PIN  D1    // _SLEEP
#define CS_PIN      A0    // CS
#define _FAULT_PIN  D10   // _FAULT

#define DWELL_MSEC      10000
#define NUM_SAMPLES     10
#define MIN_DUTY_CYCLE  10.0
#define MAX_DUTY_CYCLE  99.9
#define MIN_FREQ        500


RP2040_PWM *PWM_Instance;
OnBoardLED *neoPix;

float freq = 2000;
float dutyCycle = 50;
unsigned long lastTime = millis();
unsigned long loopCnt = 0;
float currentSense = 0;


void checkInput() {
  if (Serial.available()) {
    byte cmd = Serial.peek();
    switch (cmd) {
    case 'C':
    case 'c':
      // try to unjam the feeder by going back and forth a bit
      digitalWrite(_SLEEP_PIN, LOW);
      delay(250);  //// TODO tune this value
      digitalWrite(_SLEEP_PIN, HIGH);

      digitalWrite(DIR_PIN, LOW);
      delay(500);  //// TODO make this value a function of current speed
      digitalWrite(DIR_PIN, HIGH);
      delay(500);  //// TODO make this value a function of current speed
  
      Serial.println("Clear");
      break;
    case 'S':
      // start
      dutyCycle = MAX_DUTY_CYCLE;
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      Serial.println("Start");
      break;
    case 's':
      // stop
      dutyCycle = 0.0;
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      Serial.println("Stop");
      break;
    case 'f':
      // set frequency
      freq = Serial.parseFloat();
      freq = (freq < MIN_FREQ) ? MIN_FREQ : freq;
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      Serial.print("Freq: "); Serial.println(freq);
      break;
    default:
      dutyCycle = Serial.parseFloat();
      if (dutyCycle < MIN_DUTY_CYCLE) {
        dutyCycle = MIN_DUTY_CYCLE;
      } else if (dutyCycle >= 100.0) {
        dutyCycle = MAX_DUTY_CYCLE;
      }
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      Serial.print("Speed: "); Serial.println(dutyCycle);
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

  if (digitalRead(_FAULT_PIN) == 0) {
    neoPix->setColor(RED);
  } else {
    neoPix->setColor(GREEN);
  }

  loopCnt++;
}

  /*
  while (digitalRead(_FAULT_PIN) == 0) {
    // Over-current/-temperature: pause, backup, then continue
    digitalWrite(_SLEEP_PIN, LOW);
    delay(250);  //// TODO tune this value
    digitalWrite(_SLEEP_PIN, HIGH);

    digitalWrite(DIR_PIN, LOW);
    delay(500);  //// TODO tune this value
    digitalWrite(DIR_PIN, HIGH);
    delay(500);  //// TODO tune this value
    Serial.print(".");
  }
  */

  /*
  int cs = analogRead(CS_PIN);
  currentSense += cs;
  if (loopCnt >= NUM_SAMPLES) {
    //Serial.println(currentSense / NUM_SAMPLES);
    currentSense = 0;
  }
  */

/*
  if (Serial.available()) {
    String s = Serial.readString();
    if (s[0] == '+') {
      dutyCycle += 10.0;
      if (dutyCycle > 100.0) {
        dutyCycle = 100.0;
      }
    } else {
      if (s[0] == '-') {
        dutyCycle -= 10.0;
        if (dutyCycle < 0) {
          dutyCycle = 0.0;
        }
      }
    }
    Serial.println("");
    Serial.print("Duty Cycle: ");Serial.println(dutyCycle);
    PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
    delay(1000);
  }
*/

/*
void loop() {
  unsigned long currTime = millis();

  if (currTime >= (lastTime + DWELL_MSEC)) {
    dutyCycle += 10.0;
    if (dutyCycle > 100.0) {
      dutyCycle = 0.0;
    }
    Serial.print("Duty Cycle: ");Serial.println(dutyCycle);
    PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
    lastTime = currTime;
  }
  Serial.print(digitalRead(_FAULT_PIN));
  Serial.print(" ");
  Serial.println(analogRead(CS_PIN));
}
*/
