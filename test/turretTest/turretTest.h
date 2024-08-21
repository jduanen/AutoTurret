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
#include <ArduinoLog.h>  // fatal, error, warning, notice, trace, verbose

#include <OnBoardLED.h>


#define PWM_PIN         D3    // EN
#define DIR_PIN         D2    // PH
#define _SLEEP_PIN      D1    // _SLEEP
#define CS_PIN          A0    // CS
#define _FAULT_PIN      D10   // _FAULT
#define TRIG_PIN        D9

#define MAX_SHOTS_PER_BURST 1000  // (shots/burst) //// FIXME
#define MAX_BURST_RATE      500   // (shots/sec) //// FIXME

// N.B. Shot Interval is in units of msec
#define MIN_SHOT_INTERVAL   150   // (msec) function of the AEG and it's power supply
#define MAX_SHOT_INTERVAL   1000  // (msec) arbitrary value

// N.B. Shot Rate is shots/sec
#define MIN_SHOT_RATE   (1000.0 / MAX_SHOT_INTERVAL)  // (shot/sec) 1
#define MAX_SHOT_RATE   (1000.0 / MIN_SHOT_INTERVAL)  // (shots/sec) 6.67

// N.B. Duty Cycle is in % of motor speed
#define MIN_DUTY_CYCLE  30.0    // (%) can't feed reliably below 30%
#define MAX_DUTY_CYCLE  99.0    // (%) PWM not DC, so can't be 100%

// N.B. Feed Rate is in pellets/sec (measured times @ 19V)
#define MIN_FEED_RATE   15.4    // (pellets/sec)
#define MAX_FEED_RATE   55.6    // (pellets/sec)

#define MIN_FEED_INTERVAL   75  // (msec)

#define PRIME_PELLETS   37      // (pellets) function of the length of the feeder tube
#define PRIME_DURATION  1300    // (msec) (MAX_DUTY_CYCLE measured @ 19V)

#define MIN_FREQ        500     // (Hz) //// TODO fix this
#define MAX_FREQ        5000    // (Hz) //// TODO fix this

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
  union {
      uint32_t numShots;    // (pellets)
      uint32_t duration;    // (msec)
  } time;
  union {
    float shotRate;   // (shots/sec)
    float dutyCycle;  // (%)
  } rate;
} ShotCmd_t;

/*
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
*/


class TimedController {
public:
    virtual bool exec() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool burst(uint32_t numberOfShots, float shotRate) = 0;
    bool isActive() { return _active; };
protected:
    bool _active = false;

    uint16_t _numberOfShots;    // (shots)
    float _burstRate;           // (shots/sec)

    uint32_t _burstDuration;    // (msec)
    uint32_t _shotInterval;     // (msec)

    unsigned long _burstStartTime;  // (msec)
    unsigned long _burstEndTime;    // (msec)

    uint16_t _numberOfFeeds;    // (feeds)

    uint16_t _shotNumber;       // [0-_numberOfShots]
    uint16_t _feedNumber;       // [0-_numberOfFeeds]

    bool _burstSetup(uint32_t numberOfShots, float shotRate) {
        if (_active) {
            Serial.println("ERROR: already active, can't burst");
            return true;
        }

        _numberOfShots = _normNumberOfShots(numberOfShots);  // #shots
        _burstRate = (_normShotRate(shotRate) / 1000.0);     // #shots/msec
        _burstDuration = (_numberOfShots / _burstRate);      // msec
        _shotInterval = (1.00 / _burstRate);                 // msec
        _burstStartTime = millis();                          // msec
        _burstEndTime = (_burstStartTime + _burstDuration);  // msec
        _shotNumber = 0;
        _feedNumber = 0;

        Serial.print(_numberOfShots); Serial.print(", ");
        Serial.print(_burstRate); Serial.print(", ");
        Serial.print(_burstDuration); Serial.print(", ");
        Serial.print(_shotInterval); Serial.print(", ");
        Serial.print(_burstStartTime); Serial.print(", ");
        Serial.println(_burstEndTime);
        return false;
    };

    float _rateToDutyCycle(float burstRate) {
        // N.B. coefficents were experimentally derived
        return (1.742 * ((burstRate * 1000.0) * 1.81));
    }

    float _shotRateToDutyCycle(float shotRate) {
        // N.B. coefficents were experimentally derived
        return (1.742 * (shotRate * 1.81));
    }

    float _dutyCycleToRate(float dutyCycle) {
        return ((0.574 * dutyCycle) - 1.81);  // N.B. coefficents were experimentally derived
    }

    float _normDutyCycle(float dutyCycle) {
        float dc;

        dc = (((dutyCycle > 0) && (dutyCycle < MIN_DUTY_CYCLE)) ? MIN_DUTY_CYCLE : dutyCycle);
        dc = ((dc > MAX_DUTY_CYCLE) ? MAX_DUTY_CYCLE : dc);

        if (dc != dutyCycle) {
            Serial.print("INFO: duty cycle normalized (");
            Serial.print(dutyCycle);Serial.print(", ");
            Serial.print(dc);Serial.println(")");
        }

        return dc;  // % of full motor speed
    }

    uint32_t _normNumberOfShots(uint32_t numShots) {
        uint32_t shots = ((numShots > MAX_SHOTS_PER_BURST) ? MAX_SHOTS_PER_BURST : numShots);
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

    float _pelletsToDuration(uint32_t numPellets) {
        return ((numPellets * (float)PRIME_DURATION) / (float)PRIME_PELLETS);
    }
};

#include "FeederController.h"
#include "TriggerController.h"
