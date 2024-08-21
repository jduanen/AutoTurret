/******************************************************************************
 * 
 * AutoTurret test program
 * 
 ******************************************************************************/

#include "turretTest.h"


OnBoardLED *neoPix;

cppQueue cmdQ(sizeof(ShotCmd_t), CMD_Q_SIZE, FIFO);

FeederController *feeder;
TriggerController *trigger;


void getInput(cppQueue *qPtr) {
    if (Serial.available()) {
        ShotCmd_t cmd;
        Serial.println("--------");
        byte chr = Serial.peek();
        switch (chr) {
        case 'b':
            Serial.print("Burst: ");
            cmd.cmd = BURST;
            cmd.time.numShots = Serial.parseInt();
            cmd.rate.shotRate = Serial.parseFloat();
            Serial.print(cmd.time.numShots);Serial.print(", ");Serial.println(cmd.rate.shotRate);
            break;
        case 'C':
        case 'c':
            cmd.cmd = CLEAR;
            break;
        case 'f':
            Serial.println("Turn Feeder On");
            cmd.cmd = FEED;
            cmd.rate.dutyCycle = Serial.parseFloat();
            cmd.time.duration = Serial.parseInt();
            break;
        case 'F':
            Serial.println("Continuous Shots");
            cmd.cmd = FULL;
            cmd.rate.shotRate = Serial.parseInt();
            break;
        case 'p':
            Serial.println("Prime the Feeder");
            cmd.cmd = PRIME;
            cmd.time.numShots = PRIME_PELLETS;
            break;
        case 's':
            Serial.println("Single Shot");
            cmd.cmd = BURST;
            cmd.time.numShots = 1;
            cmd.rate.shotRate = 0.50;  //// TODO figure out appropriate rate
            break;
        case 'h':
        case '?':
            Serial.println("b <num> <rate>: burst");
            Serial.println("c: clear");
            Serial.println("f <dc> [dur]: feeder on with dutyCycle [opt duration (msec)]");
            Serial.println("F <rate>: continuous shots at rate");
            Serial.println("p: prime the feeder");
            Serial.println("s: single shot");
            Serial.println("*: feeder and trigger off");
            cmd.cmd = INVALID;
            break;
        default:
            Serial.println("Stop Shooting");
            cmd.cmd = STOP;
            break;
        }
        Serial.flush();
        if (cmd.cmd != INVALID) {
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
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);  // Error, Info, Warn, Debug, Verbose
    Log.notice("BEGIN");Serial.flush();

    // set up feeder and trigger objects
    feeder = new FeederController();
    trigger = new TriggerController();

    // set up Neo Pixel
    neoPix = new OnBoardLED(NEOPIXEL_POWER, PIN_NEOPIXEL);
    neoPix->setColor(BLACK);

    Log.notice("START");Serial.flush();
}

void loop() {
    getInput(&cmdQ);
    if (cmdQ.isEmpty() == false) {
        ShotCmd_t cmd;
        cmdQ.pop(&cmd);
        switch (cmd.cmd) {
        case BURST:
            feeder->burst(cmd.time.numShots, cmd.rate.shotRate);
            trigger->burst(cmd.time.numShots, cmd.rate.shotRate);
            break;
        case CLEAR:
            feeder->clear();
            break;
        case FEED:
            feeder->setDutyCycle(cmd.rate.dutyCycle);
            feeder->start();
            if (cmd.time.duration != 0) {
                delay(cmd.time.duration);
                feeder->stop();
            }
            break;
        case FIRE:
            trigger->start();
            break;
        case FULL:
            feeder->setDutyCycle(cmd.rate.shotRate);
            feeder->start();
            trigger->start();
            break;
        case PRIME:
            feeder->prime(cmd.time.numShots);
            break;
        case STOP:
            feeder->stop();
            trigger->stop();
            break;
        default:
            Log.error("invalid command type (0x%x)", cmd.cmd);
            break;
        }
    }

    if (feeder->exec()) {
        Log.warning("feeder exec failed");
    }
    if (trigger->exec()) {
        Log.warning("trigger exec failed");
    }

    //// FIXME add methods to FeederController?
    if (digitalRead(_FAULT_PIN) == 0) {
        if (feeder->isActive()) {
            neoPix->setColor(RED);  // fault while active
            digitalWrite(_SLEEP_PIN, LOW);
            delay(5);
            digitalWrite(_SLEEP_PIN, HIGH);
        } else {
            neoPix->setColor(MAGENTA);  // no fault while active
        }
        neoPix->setColor(BLUE);  // FIXME
    } else {
        neoPix->setColor(BLACK);  // no fault
    }
};

/*
//// TODO
void loop1() {
    feeder->exec();
};
*/
