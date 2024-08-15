/******************************************************************************
 * 
 * AutoTurret test program
 * 
 ******************************************************************************/

#include <RP2040_PWM.h>
#include <cppQueue.h>

#include <OnBoardLED.h>


#define PWM_PIN         D3    // EN
#define DIR_PIN         D2    // PH
#define _SLEEP_PIN      D1    // _SLEEP
#define CS_PIN          A0    // CS
#define _FAULT_PIN      D10   // _FAULT
#define TRIG_PIN        D9

#define MIN_FREQ        500   // TODO fix this
#define MAX_FREQ        1000  // TODO fix this

#define MIN_SHOT_RATE   1000  // 1 shot/sec
#define MAX_SHOT_RATE   150   // 7 shots/sec = 143msec

#define MIN_DUTY_CYCLE  11.0
#define MAX_DUTY_CYCLE  99.9
#define MIN_FEEDER_TIME 1000  // msec

#define PRIME_PELLETS   32   // a function of the length of the feeder tube
//#define PRIME_TIME      750  // time to prime the feeder line (@ max speed) TMP TMP TMP

#define CMD_Q_SIZE      8


typedef enum CmdType_e {
  FIRE,
  CLEAR,
  FEED,
  TRIGGER,
  STOP
} CmdType;

typedef struct {
  CmdType cmd;
  uint32_t numShots;
  union {
    float shotRate;  // shots/sec
    float dutyCycle;
  } data;
} ShotCmd_t;


bool primed;
float freq;
unsigned long triggerTime;

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

// normalize feeder duty cycle value
constexpr float normDutyCycle(float dutyCycle) {
  dutyCycle = (((dutyCycle > 0) && (dutyCycle < MIN_DUTY_CYCLE)) ? MIN_DUTY_CYCLE : dutyCycle);
  dutyCycle = ((dutyCycle > MAX_DUTY_CYCLE) ? MAX_DUTY_CYCLE : dutyCycle);
  return dutyCycle;
}

void primeFeeder(int numPellets) {
  Serial.print("p");Serial.println(numPellets);
  PWM_Instance->setPWM(PWM_PIN, freq, MAX_DUTY_CYCLE);
//  delay(numPellets * <MIN_MSEC_PER_PELLET>);
  delay(3000);
  PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
}

void fire(int numShots, float shotRate) {
  if (primed == false) {
    primeFeeder(PRIME_PELLETS);
    primed = true;
  }

  shotRate = ((shotRate < MIN_SHOT_RATE) ? MIN_SHOT_RATE : shotRate);
  shotRate = ((shotRate > MAX_SHOT_RATE) ? MAX_SHOT_RATE : shotRate);

  uint32_t duration = ((numShots / shotRate) * 1000.0);
  //// TODO deal with min feed speed
//  duration = ((duration < MIN_SHOT_TIME) ? MIN_SHOT_TIME : duration);
  startTrigger(numShots, duration);
}

void startFeeder(uint32_t shots, uint32_t duration) {
  unsigned long now = millis();
  //// FIXME select proper speed to do that many shots in the given time
//  float dutyCycle = normDutyCycle(????);
  float dutyCycle = MAX_DUTY_CYCLE;
  PWM_Instance->setPWM(PWM_PIN, freq, normDutyCycle(dutyCycle));
}

void stopFeeder() {
  Serial.println("stopFeeder");
  PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
}

void startTrigger(uint32_t shots, uint32_t duration) {
  startFeeder(shots, duration);

  //// TODO deal with min shot time
  unsigned long now = millis();
  triggerTime = now + duration;
  digitalWrite(TRIG_PIN, HIGH);
}

void stopTrigger() {
  Serial.println("stopTrigger");
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
      break;
    case 'f':
      Serial.print("Fire: ");
      cmd.cmd = FIRE;
      //// FIXME get args from console input
      cmd.numShots = Serial.parseFloat();
      cmd.data.shotRate = Serial.parseInt();
      Serial.print(cmd.numShots);Serial.print(", ");Serial.println(cmd.data.shotRate);Serial.flush();
      break;
    case 'o':
      Serial.println("Continuous");
      //// FIXME figure this one out
      cmd.cmd = FIRE;
      cmd.numShots = 1000000;
      cmd.data.shotRate = 5.0;  //// TODO figure out appropriate rate
      break;
    case 'p':
      Serial.println("Turn Feeder On");
      cmd.cmd = FEED;
      cmd.data.dutyCycle = Serial.parseFloat();
      break;
    case 's':
      Serial.println("Single Shot");Serial.flush();
      cmd.cmd = FIRE;
      cmd.numShots = 1;
      cmd.data.shotRate = 1.0;  //// TODO figure out appropriate rate
      break;
    default:
      Serial.println("Stop Shooting");
      cmd.cmd = STOP;
      break;
    }
    qPtr->push(&cmd);

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

  freq = 2000.0;  //// TODO figure out the right value for this
  primed = false;
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

  Serial.println("START");Serial.flush();
}

void loop() {
  getInput(&cmdQ);
  if (cmdQ.isEmpty() == false) {
    float dutyCycle;
    ShotCmd_t cmd;
    cmdQ.pop(&cmd);
    switch (cmd.cmd) {
    case CLEAR:
      clearFeeder();
      break;
    case FIRE:
      fire(cmd.numShots, cmd.data.shotRate);
      break;
    case FEED:
      dutyCycle = normDutyCycle(cmd.data.dutyCycle);
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      break;
    case STOP:
      stopFeeder();
      stopTrigger();
      break;
    default:
      Serial.print("ERROR: invalid command type ("); Serial.print(cmd.cmd);Serial.println(")");
      break;
    }
  }

  unsigned long now = millis();  // N.B. rolls over after ~50 days of uptime
  //// TODO deal with rollover
  if (triggerTime && (triggerTime <= now)) {
    Serial.print(now);Serial.println(triggerTime);
    stopTrigger();
  }

  if (digitalRead(_FAULT_PIN) == 0) {
    if (triggerTime) {
      neoPix->setColor(RED);  // fault while active
      digitalWrite(_SLEEP_PIN, LOW);
      delay(5);
      digitalWrite(_SLEEP_PIN, HIGH);
    } else {
      neoPix->setColor(MAGENTA);  // no fault while active
    }
  } else {
    neoPix->setColor(BLACK);  // no fault
  }
}
