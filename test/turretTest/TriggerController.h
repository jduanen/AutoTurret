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
	TriggerController(BurstState_t *burstPtr) {
		Serial.println("TriggerController");
    	pinMode(TRIG_PIN, OUTPUT);
	    digitalWrite(TRIG_PIN, LOW);
	};

	bool exec() {
		Serial.println("Trigger Exec");
		return true;
	};

	void start(uint32_t numShots, float shotRate) {
	    Serial.println("Trigger Start");
	    startFeeder(numShots, shotRate);
//            numShots = normNumShots(numShots);
//            shotRate = normShotRate(shotRate);
	    //// TODO deal with min shot time
	    unsigned long now = millis();
	    uint32_t duration = ((numShots * 1000.0) / shotRate);  // msec
	    triggerTime = now + duration;
	    digitalWrite(TRIG_PIN, HIGH);
	};

	void stop() {
	    Serial.println("Trigger Stop");
	    digitalWrite(TRIG_PIN, LOW);
	    triggerTime = 0;
	    stopFeeder();
	};

	bool isOn() { return _triggerOn; };
protected:
	bool _tiggerOn = false;
};
