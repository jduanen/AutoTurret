/******************************************************************************
 * 
 * AutoTurret Feeder headers file
 * 
 ******************************************************************************/

class FeederController : public TimedController {
public:
	FeederController() {
		Serial.println("FeederController");
		pinMode(DIR_PIN, OUTPUT);
	    pinMode(_SLEEP_PIN, OUTPUT);
	    pinMode(CS_PIN, INPUT);  // ANALOG
	    pinMode(_FAULT_PIN, INPUT); //, INPUT_PULLUP);

		digitalWrite(DIR_PIN, HIGH);
	    digitalWrite(_SLEEP_PIN, HIGH);

	    _pwm = new RP2040_PWM(PWM_PIN, _freq, 0.0);
	    _pwm->setPWM(PWM_PIN, _freq, 0.0);
	};

	bool start() {
	    Serial.println("Feeder Start");
	    if (_active) {
	    	Serial.println("ERROR: already active, can't start");
	    	return true;
	    }
	    Serial.print(_freq);Serial.print(", ");Serial.println(_dutyCycle);
        _pwm->setPWM(PWM_PIN, _freq, _dutyCycle);
        _active = (_dutyCycle > 0.0);
        return false;
	}

	bool stop() {
	    Serial.println("Feeder Stop");
	    _pwm->setPWM(PWM_PIN, _freq, 0.0);
	    _active = false;
	    return false;
	}

	bool exec() {
//		Serial.println("Feeder Exec");
		if (!_active) {
			return false;
		}
		//// TODO
		return false;
	};

	bool burst(uint32_t numberOfShots, float shotRate) {
		Serial.println("Feeder Burst");
	    if (_burstSetup(numberOfShots, shotRate)) {
            return true;
        }
	    /*
		float dutyCycle = MAX_DUTY_CYCLE;
	    dutyCycle = rateToDutyCycle(shotRate);
	    dutyCycle = normDutyCycle(dutyCycle);
	    _pwm->setPWM(PWM_PIN, _freq, dutyCycle);
	    */
	    return false;
	}

	bool prime(uint16_t numPellets) {
		if (_active) {
			Serial.println("ERROR: already active, can't prime");
	    	return true;
		}
		if (_primed) {
			Serial.println("INFO: already primed");
			return false;
		}
		Serial.print("Feeder Prime: ");Serial.print(numPellets);Serial.print(", ");
	    _pwm->setPWM(PWM_PIN, _freq, MAX_DUTY_CYCLE);
	    delay(numPellets * MAX_FEED_RATE);
		Serial.println(numPellets * MAX_FEED_RATE);
	    _pwm->setPWM(PWM_PIN, _freq, 0.0);
	    _primed = true;
	    return false;
	}

	// try to unjam the feeder by going back and forth a bit
	void clear() {
	    Serial.println("Feeder Clear");
		stop();

	    _pwm->setPWM(PWM_PIN, _freq, 0.0);
	    digitalWrite(_SLEEP_PIN, LOW);
	    delay(5);
	    digitalWrite(_SLEEP_PIN, HIGH);

	    digitalWrite(DIR_PIN, LOW);
	    _pwm->setPWM(PWM_PIN, _freq, MAX_DUTY_CYCLE);
	    delay(250);  //// TODO tune this value
	    digitalWrite(DIR_PIN, HIGH);
	    delay(250);  //// TODO tune this value
	    _pwm->setPWM(PWM_PIN, _freq, 0.0);
	}

	bool isPrimed() { return _primed; };

	void setFrequency(float freq) { _freq = freq; };

	float getFrequency() { return _freq; };

	void setDutyCycle(float dutyCycle) { _dutyCycle = _normDutyCycle(dutyCycle); };

	float getDutyCycle() { return _dutyCycle; };

protected:
	float _freq = 2000;		//// FIXME 
	float _dutyCycle = 0.0;
	bool _primed = false;

	RP2040_PWM *_pwm;
};
