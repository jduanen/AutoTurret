/******************************************************************************
 * 
 * AutoTurret test program headers file
 * 
 * Nomenclature:
 *  - Burst (numberOfShots, shotRate)
 *    * one or more shots with time between them defined by a given rate of fire
 *  - Rate (shots/sec)
 *    * number of shots per second
 *  - Shot
 *    * firing of a single pellet with feeding
 *    * includes the time to fire and any associated delay
 *      (that defines when the next shot can be fired)
 *  - Interval (1000/Rate)
 *    * the duration of a Shot in a Burst (in msec)
 *  - Time (msec)
 *    * a value in the range of values given by the millis() function
 *    * all variables that end in 'Time' refer to a time in this range
 *
 ******************************************************************************/

#pragma once

#include <RP2040_PWM.h>
#include <cppQueue.h>

#include <OnBoardLED.h>


#define PWM_PIN         D3    // EN
#define DIR_PIN         D2    // PH
#define _SLEEP_PIN      D1    // _SLEEP
#define CS_PIN          A0    // CS
#define _FAULT_PIN      D10   // _FAULT
#define TRIG_PIN        D9

#define MAX_SHOTS_PER_BURST 1000  //// FIXME
#define MAX_BURST_RATE      500   //// FIXME

// N.B. Shot Interval is in units of msec
#define MIN_SHOT_INTERVAL   150   // a function of the AEG and it's power supply
#define MAX_SHOT_INTERVAL   1000  // arbitrary value

// N.B. Shot Rate is shots/sec
#define MIN_SHOT_RATE   (1000.0 / MAX_SHOT_INTERVAL)  // 1 shot/sec
#define MAX_SHOT_RATE   (1000.0 / MIN_SHOT_INTERVAL)  // 6.67 shots/sec

// N.B. Duty Cycle is in % of motor speed
#define MIN_DUTY_CYCLE  30.0  // can't feed reliably below 30%
#define MAX_DUTY_CYCLE  99.0 // PWM not DC, so can't be 100%

// N.B. Feed Rate is in pellets/sec
#define MIN_FEED_RATE   15.4  // pellets/sec
#define MAX_FEED_RATE   55.6  // pellets/sec

#define PRIME_PELLETS   32    // a function of the length of the feeder tube

#define MIN_FREQ        500   // TODO fix this
#define MAX_FREQ        5000  // TODO fix this

#define CMD_Q_SIZE      8


// command message types
typedef enum CmdType_e {
  BURST,
  CLEAR,
  FEED,
  FIRE,
  FULL,
  PRIME,
  STOP,
  INVALID
} CmdType;

// command messages to be passed to the main loop via a queue
typedef struct {
  CmdType cmd;
  uint32_t numShots;
  union {
    float shotRate;   // shots/sec
    float dutyCycle;  // %
  } data;
} ShotCmd_t;

// global state describing a burst of (one or more) shots
//// TODO fix up types/sizes
typedef struct {
    uint16_t        numberOfShots;  // (pellets)
    float           burstRate;      // (pellets/sec)

    uint32_t        burstDuration;  // (msec)
    uint32_t        shotInterval;   // (msec)

    uint16_t        shotNumber;     // [0-numberOfShots]
    unsigned long   burstStartTime; // (msec)
    unsigned long   burstEndTime;   // (msec)
} BurstState_t;


class TimedController {
public:
    virtual bool exec() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool burst(uint32_t numberOfShots, float shotRate) = 0;
    bool isActive() { return _active; };
protected:
    bool _active = false;

    uint16_t _numberOfShots;    // (pellets)
    float _burstRate;           // (pellets/sec)

    uint32_t _burstDuration;    // (msec)
    uint32_t _shotInterval;     // (msec)

    unsigned long _burstStartTime;  // (msec)
    unsigned long _burstEndTime;    // (msec)

    uint16_t _shotNumber;       // [0-numberOfShots]

    bool _burstSetup(uint32_t numberOfShots, float shotRate) {
        if (_active) {
            Serial.println("ERROR: already active, can't burst");
            return true;
        }

        _numberOfShots = numberOfShots;
        _burstRate = (shotRate * 1000.0);
        _burstDuration = ((numberOfShots * 1000.0) / _burstRate);
        _shotInterval = (1000.00 / _burstRate);
        _burstStartTime = millis();
        _burstEndTime = (_burstStartTime + _burstDuration);
        _shotNumber = 0;

        Serial.print(_numberOfShots); Serial.print(", ");
        Serial.print(_burstRate); Serial.print(", ");
        Serial.print(_burstDuration); Serial.print(", ");
        Serial.print(_shotInterval); Serial.print(", ");
        Serial.print(_burstStartTime); Serial.print(", ");
        Serial.print(_burstEndTime); Serial.print(", ");
        Serial.println(_shotNumber);

        _active = true;
        return false;
    };

    float _rateToDutyCycle(float shotRate) {
        return (1.742 * (shotRate * 1.81));  // N.B. coefficents were experimentally derived
    }

    float _dutyCycleToRate(float dutyCycle) {
        return ((0.574 * dutyCycle) - 1.81);  // N.B. coefficents were experimentally derived
    }

    float _normDutyCycle(float dutyCycle) {
        float dc;

        dc = (((dutyCycle > 0) && (dutyCycle < MIN_DUTY_CYCLE)) ? MIN_DUTY_CYCLE : dutyCycle);
        dc = ((dc > MAX_DUTY_CYCLE) ? MAX_DUTY_CYCLE : dc);

        if (dc != dutyCycle) {
            Serial.println("INFO: duty cycle normalized");
        }

        return dc;  // % of full motor speed
    }

    uint32_t _normNumShots(uint32_t numShots) {
        uint32_t shots = ((shots > MAX_SHOTS_PER_BURST) ? MAX_SHOTS_PER_BURST : shots);
        if (shots != numShots) {
            Serial.println("INFO: number of shots normalized");
        }
        return shots;
    };

    float _normShotRate(float shotRate) {
        float rate = ((shotRate < MIN_SHOT_RATE) ? MIN_SHOT_RATE : shotRate);
        rate = ((rate > MAX_SHOT_RATE) ? MAX_SHOT_RATE : rate);

        if (rate != shotRate) {
            Serial.println("INFO: shot rate normalized");
        }
        return shotRate;  // shots/sec
    };
};

#include "FeederController.h"
#include "TriggerController.h"
