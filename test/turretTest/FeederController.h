/******************************************************************************
 * 
 * AutoTurret Feeder headers file
 * 
 ******************************************************************************/

class FeederController : public TimedController {
public:
	FeederController() {
		Log.notice("FeederController");
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
//	    Log.notice("Feeder Start: %d\n", _dutyCycle);
		Serial.print("Feeder Start: ");Serial.println(_dutyCycle);
		return _start(_dutyCycle);
	}

	bool stop() {
	    Log.notice("Feeder Stop");
	    _pwm->setPWM(PWM_PIN, _freq, 0.0);
	    _active = _on = false;
	    _feedNumber = 0;
	    return false;
	}

	// try to unjam the feeder by going back and forth a bit
	void clear() {
	    Log.notice("Feeder Clear");
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
	    //// N.B. these values result in 3-6 pellets into feeder
	}

	// prime the feeder tube with pellets
	bool prime(uint16_t numPellets) {
		if (_active) {
			Log.error("already active, can't prime");
	    	return true;
		}
		if (_primed) {
			Log.notice("already primed");
			return false;
		}
	    _pwm->setPWM(PWM_PIN, _freq, MAX_DUTY_CYCLE);
	    //// TODO make sure that this works correctly with different numPellets
	    delay(static_cast<int>(std::round(_pelletsToDuration(numPellets))));
	    _pwm->setPWM(PWM_PIN, _freq, 0.0);
	    _primed = true;
	    return false;
	}

	bool burst(uint32_t numberOfShots, float shotRate) {
		Log.notice("Feeder Burst");
	    if (_burstSetup(numberOfShots, shotRate)) {
            return true;
        }
        if (!_primed) {
            prime(PRIME_PELLETS);
            _primed = true;
        }

		_dutyCycle = _rateToDutyCycle(_burstRate);
		Serial.print("dc: ");Serial.println(_dutyCycle);
		_dutyCycle = (_dutyCycle > MAX_DUTY_CYCLE) ? MAX_DUTY_CYCLE : _dutyCycle;
		Serial.print("dc2: ");Serial.println(_dutyCycle);
		if (_dutyCycle >= MIN_DUTY_CYCLE) {
			if (_start(MAX_DUTY_CYCLE)) {
				return true;
			}
			_nextTime = (_burstStartTime + MIN_FEED_INTERVAL);
			_numberOfFeeds = <int>
			Serial.print("First time: ");Serial.println(_nextTime);
		} else {
	        //// FIXME figure out dutyCycle and nextTime
			Serial.println("Duty Cycle below MIN");
			return true;
		}
	    return false;
	}

	bool exec() {
//		Serial.println("Feeder Exec");
		if (!_active) {
			return false;
		}
		unsigned long now = millis();
		if (now >= _nextTime) {
			if (_on) {
				Serial.println(now);
				if (now >= _burstEndTime) {
					if (stop()) {
						return true;
					}
				} else {
					if (_start(_dutyCycle)) {
						return true;
					}
					_next_time = _burstEndTime;
				}
			}
		}
		return false;
	};

	bool isPrimed() { return _primed; };

	void setFrequency(float freq) { _freq = freq; };

	float getFrequency() { return _freq; };

	void setDutyCycle(float dutyCycle) {
		_dutyCycle = _normDutyCycle(dutyCycle);
		Log.notice("_dutyCycle = %0.2f", _dutyCycle);
	};

	float getDutyCycle() { return _dutyCycle; };

protected:
	float _freq = 2000;		//// FIXME 
	float _dutyCycle = 0.0;
	bool _primed = false;
	bool _on = false;
	unsigned long _nextTime = 0;

	RP2040_PWM *_pwm;

	bool _start(float dutyCycle) {
	    if (_active) {
	    	Log.error("already active, can't start");
	    	return true;
	    }
        _pwm->setPWM(PWM_PIN, _freq, _dutyCycle);
        _feedNumber++;
        _active = _on = (dutyCycle > 0.0);
        return false;
	};
};
