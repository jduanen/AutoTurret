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
#define MAX_FREQ        5000  // TODO fix this

// N.B. Shot Interval is in units of msec
#define MIN_SHOT_INTERVAL   150   // a function of the AEG and it's power supply
#define MAX_SHOT_INTERVAL   1000  // arbitrary value

// N.B. Shot Rate is shots/sec
#define MIN_SHOT_RATE       (1000.0 / MAX_SHOT_INTERVAL)  // 1 shot/sec
#define MAX_SHOT_RATE       (1000.0 / MIN_SHOT_INTERVAL)  // 6.67 shots/sec

// N.B. Duty Cycle is in % of motor speed
#define MIN_DUTY_CYCLE  30.0  // can't feed reliably below 30%
#define MAX_DUTY_CYCLE  99.99 // PWM not DC, so can't be 100%

// N.B. Feed Rate is in pellets/sec
#define MIN_FEED_RATE   15.4  // pellets/sec
#define MAX_FEED_RATE   55.6  // pellets/sec

#define PRIME_PELLETS   32    // a function of the length of the feeder tube

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
    float shotRate;   // shots/sec
    float dutyCycle;  // %
  } data;
} ShotCmd_t;


bool primed;
float freq;
unsigned long feederTime;
unsigned long triggerTime;

RP2040_PWM *PWM_Instance;

OnBoardLED *neoPix;

cppQueue cmdQ(sizeof(ShotCmd_t), CMD_Q_SIZE, FIFO);


//// TODO make much of these functions inline/constexpr

constexpr float rateToDutyCycle(float shotRate) {
  return (1.742 * (shotRate * 1.81));
}

constexpr float dutyCycleToRate(float dutyCycle) {
  return ((0.574 * dutyCycle) - 1.81);
}

constexpr float normDutyCycle(float dutyCycle) {
  float dc;

  dc = (((dutyCycle > 0) && (dutyCycle < MIN_DUTY_CYCLE)) ? MIN_DUTY_CYCLE : dutyCycle);
  dc = ((dc > MAX_DUTY_CYCLE) ? MAX_DUTY_CYCLE : dc);

  if (dc != dutyCycle) {
    Serial.println("INFO: duty cycle normalized");
  }

  return dc;  // % of full motor speed
}

constexpr uint32_t normNumShots(uint32_t numShots) {
  uint32_t shots;

  shots = ((numShots < MIN_NUM_SHOTS) ? MIN_NUM_SHOTS : numShots);
  shots = ((shots > MAX_NUM_SHOTS) ? MAX_NUM_SHOTS : shots);

  if (shots != numShots) {
    Serial.println("INFO: number of shots normalized");
  }

  return shots;
}

constexpr float normShotRate(float shotRate) {
  float rate;

  rate = ((shotRate < MIN_SHOT_RATE) ? MIN_SHOT_RATE : shotRate);
  rate = ((rate > MAX_SHOT_RATE) ? MAX_SHOT_RATE : rate);

  if (rate != shotRate) {
    Serial.println("INFO: shot rate normalized");
  }

  return shotRate;  // shots/sec
}

void primeFeeder(uint32_t numPellets) {
  Serial.print("p: ");Serial.print(numPellets);Serial.print(", ");
  PWM_Instance->setPWM(PWM_PIN, freq, MAX_DUTY_CYCLE);
  delay(numPellets * MAX_FEED_RATE);
  Serial.println(numPellets * MAX_FEED_RATE);
  PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
}

void fire(uint32_t numShots, float shotRate) {
  uint32_t duration;

  numShots = normNumShots(numShots);
  shotRate = normShotRate(shotRate);

  if (primed == false) {
    primeFeeder(PRIME_PELLETS);
    primed = true;
  }

  startTrigger(numShots, shotRate);
}

void startFeeder(uint32_t pellets, float shotRate) {
  unsigned long now = millis();

  //// FIXME select proper speed to do that many shots in the given time
  uint32_t duration = ?(shotRate);
  duration = ?

  float dutyCycle = MAX_DUTY_CYCLE;
//  dutyCycle = rateToDutyCycle(shotRate);
  dutyCycle = normDutyCycle(dutyCycle);
  PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);

  feederTime = now + duration;
}

void stopFeeder() {
  Serial.println("stopFeeder");
  PWM_Instance->setPWM(PWM_PIN, freq, 0.0);
  feederTime = 0;
}

void startTrigger(uint32_t numShots, float shotRate) {
  startFeeder(numShots, shotRate);

  //// TODO deal with min shot time
  unsigned long now = millis();
  uint32_t duration = ((numShots * 1000.0) / shotRate);  // msec
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
    Serial.print(now);Serial.print(", ");Serial.println(triggerTime);
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
