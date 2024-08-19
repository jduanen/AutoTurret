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
            cmd.numShots = Serial.parseInt();
            cmd.data.shotRate = Serial.parseFloat();
            Serial.print(cmd.numShots);Serial.print(", ");Serial.println(cmd.data.shotRate);Serial.flush();
            break;
        case 'C':
        case 'c':
            cmd.cmd = CLEAR;
            break;
        case 'f':
            Serial.println("Turn Feeder On");
            cmd.cmd = FEED;
            cmd.data.dutyCycle = Serial.parseFloat();
            break;
        case 'F':
            Serial.println("Continuous Shots");
            //// FIXME figure this one out
            cmd.cmd = FULL;
            cmd.data.shotRate = Serial.parseInt();
            break;
        case 'p':
            Serial.println("Prime the Feeder");Serial.flush();
            cmd.cmd = PRIME;
            cmd.numShots = PRIME_PELLETS;
            break;
        case 's':
            Serial.println("Single Shot");Serial.flush();
            cmd.cmd = BURST;
            cmd.numShots = 1;
            cmd.data.shotRate = 1.0;  //// TODO figure out appropriate rate
            break;
        case 'h':
        case '?':
            Serial.println("b <num> <rate>: burst");
            Serial.println("c: clear");
            Serial.println("f <dc>: feeder on (with dutyCycle)");
            Serial.println("F <rate>: continuous shots at rate");
            Serial.println("p: prime the feeder");
            Serial.println("s: single shot");
            Serial.println("<*>: feeder and trigger off");
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
    Serial.println("BEGIN");

    // set up feeder and trigger objects
    feeder = new FeederController();
    trigger = new TriggerController();

    // set up Neo Pixel
    neoPix = new OnBoardLED(NEOPIXEL_POWER, PIN_NEOPIXEL);
    neoPix->setColor(BLACK);

    Serial.println("START");Serial.flush();
}

void loop() {
    getInput(&cmdQ);
    if (cmdQ.isEmpty() == false) {
        ShotCmd_t cmd;
        cmdQ.pop(&cmd);
        switch (cmd.cmd) {
        case BURST:
            feeder->burst(cmd.numShots, cmd.data.shotRate);
            trigger->burst(cmd.numShots, cmd.data.shotRate);
            break;
        case CLEAR:
            feeder->clear();
            break;
        case FEED:
            feeder->setDutyCycle(cmd.data.dutyCycle);
            feeder->start();
            break;
        case FIRE:
            trigger->start();
            break;
        case FULL:
            feeder->setDutyCycle(cmd.data.shotRate);
            feeder->start();
            trigger->start();
            break;
        case PRIME:
            feeder->prime(cmd.numShots);
            break;
        case STOP:
            feeder->stop();
            trigger->stop();
            break;
        default:
            Serial.print("ERROR: invalid command type (0x");
            Serial.print(String(cmd.cmd, HEX));
            Serial.println(")");
            break;
        }
    }

    feeder->exec();  //// TODO
    trigger->exec();

    if (digitalRead(_FAULT_PIN) == 0) {
        /*
        if (????) {
            neoPix->setColor(RED);  // fault while active
            digitalWrite(_SLEEP_PIN, LOW);
            delay(5);
            digitalWrite(_SLEEP_PIN, HIGH);
        } else {
            neoPix->setColor(MAGENTA);  // no fault while active
        }
        */
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