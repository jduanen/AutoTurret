/******************************************************************************
 * 
 * AutoTurret Trigger headers file
 * 
 ******************************************************************************/

class TriggerController : public TimedController {
public:
	TriggerController() {
		Serial.println("TriggerController");
    	pinMode(TRIG_PIN, OUTPUT);
	    digitalWrite(TRIG_PIN, LOW);
	    _active = false;
	};

	bool start() {
	    Serial.println("Trigger Start");
	    if (_active) {
	    	Serial.println("ERROR: already active, can't start");
	    	return true;
	    }
	    digitalWrite(TRIG_PIN, HIGH);
	    _active = true;
	    return false;
	};

	bool stop() {
	    Serial.println("Trigger Stop");
	    digitalWrite(TRIG_PIN, LOW);
	    _active = false;
	    return false;
	};

	bool exec() {
//		Serial.println("Trigger Exec");
		if (!_active) {
			return false;
		}
		//// TODO
		return false;
	};

	bool burst(uint32_t numberOfShots, float shotRate) {
	    Serial.println("Trigger Burst");
	    if (_burstSetup(numberOfShots, shotRate)) {
            return true;
        }
/*
//            numShots = normNumShots(numShots);
//            shotRate = normShotRate(shotRate);
	    //// TODO deal with min shot time
	    uint32_t duration = ((numShots * 1000.0) / shotRate);  // msec
	    triggerTime = millis() + duration;
	    digitalWrite(TRIG_PIN, HIGH);
	    _triggerOn = true;
*/
	    return false;
	};
};
