/******************************************************************************
 * 
 * AutoTurret test program
 * 
 ******************************************************************************/

#include <RP2040_PWM.h>
#include <cppQueue.h>

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
#define MIN_FEEDER_DUTY_CYCLE 25.0

#define TRIGGER_TIME    150  // 7 shots/sec = 143msec

#define PRIME_PELLETS   32   // a function of the length of the feeder tube
#define PRIME_TIME      750  // time to prime the feeder line (@ max speed) TMP TMP TMP

#define CMD_Q_SIZE      8


typedef enum CmdType_e {
  FIRE,
  CLEAR,
  STOP
} CmdType;

typedef struct {
  CmdType cmd;
  uint32_t numShots;
  float shotRate;  // shots/sec
} ShotCmd_t;


float freq;
float dutyCycle;
unsigned long lastTime;
unsigned int numShots;
bool primed;
unsigned long feederTime;
unsigned long triggerTime;
unsigned long loopCnt = 0;

RP2040_PWM *PWM_Instance;

OnBoardLED *neoPix;

cppQueue cmdQ(sizeof(ShotCmd_t), CMD_Q_SIZE, FIFO);


//// TODO make much of these functions inline/constexpr
/*
constexpr int foo = 1;
constexpr void bar(int foo) {
  return;
}
*/

void primeFeeder(int numPellets) {
  Serial.println("p");
  PWM_Instance->setPWM(PWM_PIN, freq, FEEDER_DUTY_CYCLE);
  delay(PRIME_TIME);  //// FIXME compute delay based on number of pellets
  PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
}

void fire(int numPellets, float pelletRate) {
  if (primed == false) {
    primeFeeder(PRIME_PELLETS);
    primed = true;
  }

  //// TODO deal with min feed speed
  uint32_t time = ((numPellets / pelletRate) * 1000.0);
  startTrigger(numPellets, time);
}

void stop() {
  stopFeeder();
  stopTrigger();
}

void startFeeder(uint32_t shots, uint32_t duration) {
  //// TODO deal with min feeder speed
  unsigned long now = millis();
  feederTime = now + ((duration < MIN_FEEDER_DUTY_CYCLE) ? MIN_FEEDER_DUTY_CYCLE : duration);
  PWM_Instance->setPWM(PWM_PIN, freq, MAX_DUTY_CYCLE);  //// FIXME select proper speed
}

void stopFeeder() {
  PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
  feederTime = 0;
}

void startTrigger(uint32_t shots, uint32_t duration) {
  startFeeder(shots, duration);

  //// TODO deal with min shot time
  unsigned long now = millis();
  triggerTime = now + duration;
  digitalWrite(TRIG_PIN, HIGH);
}

void stopTrigger() {
  digitalWrite(TRIG_PIN, LOW);
  triggerTime = 0;
  stopFeeder();
}

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

void getInput(cppQueue *qPtr) {
  if (Serial.available()) {
    ShotCmd_t cmd;
    byte chr = Serial.peek();
    switch (chr) {
    case 'C':
    case 'c':
      cmd.cmd = CLEAR;
      qPtr->push(&cmd);
      break;
    case 'f':
      Serial.print("Fire: ");
      cmd.cmd = FIRE;
      //// FIXME get args from console input
      cmd.numShots = Serial.parseFloat();
      cmd.shotRate = Serial.parseInt();
      Serial.print(cmd.numShots);Serial.print(", ");Serial.println(cmd.shotRate);
      qPtr->push(&cmd);
      break;
    case 'o':
      Serial.println("Continuous");
      //// FIXME figure this one out
      cmd.cmd = FIRE;
      cmd.numShots = 1000000;
      cmd.shotRate = 5.0;  //// TODO figure out appropriate rate
      qPtr->push(&cmd);
      break;
    case 's':
      Serial.println("Single Shot");
      cmd.cmd = FIRE;
      cmd.numShots = 1;
      cmd.shotRate = 1.0;  //// TODO figure out appropriate rate
      qPtr->push(&cmd);
      break;
    default:
      Serial.println("Stop Shooting");
      cmd.cmd = STOP;
      qPtr->push(&cmd);
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
  feederTime = 0;
  triggerTime = 0;

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

void loop() {
  getInput(&cmdQ);
  if (cmdQ.isEmpty() == false) {
    ShotCmd_t cmd;
    cmdQ.pop(&cmd);
    switch (cmd.cmd) {
    case CLEAR:
      clearFeeder();
      break;
    case FIRE:
      fire(cmd.numShots, cmd.shotRate);
      break;
    case STOP:
      stop();
    default:
      Serial.print("ERROR: invalid command type - "); Serial.println(cmd.cmd);
      break;
    }
  }

  unsigned long now = millis();  // N.B. rolls over after ~50 days of uptime
  //// TODO deal with rollover
  if (feederTime && (feederTime <= now)) {
    stopFeeder();
  }
  if (triggerTime && (triggerTime <= now)) {
    stopTrigger();
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

    /*
    case 'd':
      dutyCycle = Serial.parseFloat();
      if (dutyCycle < MIN_DUTY_CYCLE) {
        dutyCycle = 0.0;
      } else if (dutyCycle >= 100.0) {
        dutyCycle = MAX_DUTY_CYCLE;
      }
      Serial.print("Speed: "); Serial.println(dutyCycle);
      break;
    */

  /*
  if (numShots > 0) {
    if (primed == false) {
      primeFeeder(PRIME_PELLETS);
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
  */
