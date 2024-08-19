/******************************************************************************
 * 
 * AutoTurret Trigger headers file
 * 
 ******************************************************************************/

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
		Serial.println("Trigger Exec");
		if (!_active) {
			return false;
		}
		//// TODO
		return false;
	};

	bool _burstHandler() {
	    Serial.println("Trigger Burst Handler");
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
protected:
};
