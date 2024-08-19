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
		digitalWrite(DIR_PIN, HIGH);
	    pinMode(_SLEEP_PIN, OUTPUT);
	    digitalWrite(_SLEEP_PIN, LOW);
	    pinMode(CS_PIN, INPUT);  // ANALOG
	    pinMode(_FAULT_PIN, INPUT); //, INPUT_PULLUP);
	    digitalWrite(_SLEEP_PIN, HIGH);

	    _pwm = new RP2040_PWM(PWM_PIN, freq, 0.0);
	};

	bool start() {
	    Serial.println("Feeder Start");
	    if (_active) {
	    	Serial.println("ERROR: already active, can't start");
	    	return true;
	    }
        _pwm->setPWM(PWM_PIN, freq, MAX_DUTY_CYCLE);
//        feederTime = 0;
        _active = true;
        return false;
	}

	bool stop() {
	    Serial.println("Feeder Stop");
	    _pwm->setPWM(PWM_PIN, freq, 0.0);
	    feederTime = 0;
	    _active = false;
	    return false;
	}

	bool exec() {
		Serial.println("Feeder Exec");
		if (!_active) {
			return false;
		}
		//// TODO
		return false;
	};

	bool _burstHandler() {
		Serial.println("Feeder Burst Handler");
	    /*
		float dutyCycle = MAX_DUTY_CYCLE;
	    dutyCycle = rateToDutyCycle(shotRate);
	    dutyCycle = normDutyCycle(dutyCycle);
	    _pwm->setPWM(PWM_PIN, freq, dutyCycle);
	    */
	    return false;
	}

	bool prime(uint16_t numPellets) {
		if (_active) {
			Serial.println("ERROR: already active, can't prime");
	    	return true;
		}
		Serial.print("Feeder Prime: ");Serial.print(numPellets);Serial.print(", ");

	    _pwm->setPWM(PWM_PIN, freq, MAX_DUTY_CYCLE);
	    delay(numPellets * MAX_FEED_RATE);
		Serial.println(numPellets * MAX_FEED_RATE);
	    _pwm->setPWM(PWM_PIN, freq, 0.0);

	    _primed = true;
	    return false;
	}

	// try to unjam the feeder by going back and forth a bit
	void clear() {
	    Serial.println("Feeder Clear");
		stop();

	    _pwm->setPWM(PWM_PIN, freq, 0.0);
	    digitalWrite(_SLEEP_PIN, LOW);
	    delay(5);
	    digitalWrite(_SLEEP_PIN, HIGH);

	    digitalWrite(DIR_PIN, LOW);
	    _pwm->setPWM(PWM_PIN, freq, MAX_DUTY_CYCLE);
	    delay(250);  //// TODO tune this value
	    digitalWrite(DIR_PIN, HIGH);
	    delay(250);  //// TODO tune this value
	    _pwm->setPWM(PWM_PIN, freq, 0.0);
	}

	bool isPrimed() { return _primed; };

	void setFrequency(float freq) { _freq = freq; };

	float getFrequency() { return _freq; };
protected:
	float _freq = 2000;		//// FIXME 
	bool _primed = false;

	RP2040_PWM *_pwm;

	float _rateToDutyCycle(float shotRate) {
	    return (1.742 * (shotRate * 1.81));
	}

	float _dutyCycleToRate(float dutyCycle) {
    	return ((0.574 * dutyCycle) - 1.81);
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
};
