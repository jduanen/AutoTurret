/******************************************************************************
 * 
 * AutoTurret test program
 * 
 ******************************************************************************/

#include "turretTest.h"


OnBoardLED *neoPix;

cppQueue cmdQ(sizeof(ShotCmd_t), CMD_Q_SIZE, FIFO);

BurstState_t  burst;

FeederController *feeder;
TriggerController *trigger;

//// TODO make much of these functions inline/constexpr

void getInput(cppQueue *qPtr) {
    if (Serial.available()) {
        ShotCmd_t cmd;
        byte chr = Serial.peek();
        switch (chr) {
        case 'C':
        case 'c':
            cmd.cmd = CLEAR;
            break;
        case 'b':
            Serial.print("Burst: ");
            cmd.cmd = BURST;
            cmd.numShots = Serial.parseFloat();
            cmd.data.shotRate = Serial.parseInt();
            Serial.print(cmd.numShots);Serial.print(", ");Serial.println(cmd.data.shotRate);Serial.flush();
            break;
        case 'o':
            Serial.println("Continuous");
            //// FIXME figure this one out
            cmd.cmd = BURST;
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
            cmd.cmd = BURST;
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

    // set up feeder and trigger objects
    burst = {
        0,    // (pellets)
        0.0,  // (pellets/sec)
        0,    // (msec)
        0,    // (msec)
        0,    // [0-numberOfShots]
        0,    // (msec)
        0,    // (msec)
    };
    feeder = new FeederController(&burst);
    trigger = new TriggerController(&burst);

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
            feeder->clear();
            break;
        case BURST:
            // void burst(uint32_t numShots, float shotRate) {
            // burst(cmd.numShots, cmd.data.shotRate);
            startTrigger(numShots, shotRate);
            break;
        case FEED:
            feeder->start(?);
            break;
        case FIRE:
            trigger->start(?);
            break;
        case STOP:
            feeder->stop();
            trigger->stop();
            break;
        default:
            Serial.print("ERROR: invalid command type ("); Serial.print(cmd.cmd);Serial.println(")");
            break;
        }
    }

/*
  unsigned long now = millis();  // N.B. rolls over after ~50 days of uptime
  //// TODO deal with rollover
  if (triggerTime && (triggerTime <= now)) {
    Serial.print(now);Serial.print(", ");Serial.println(triggerTime);
    stopTrigger();
  }
*/

    feeder->exec();
    trigger->exec();

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
