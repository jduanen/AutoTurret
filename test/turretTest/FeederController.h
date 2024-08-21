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
        _on = (_dutyCycle > 0.0);
        return false;
	}

	bool stop() {
	    Serial.println("Feeder Stop");
	    _pwm->setPWM(PWM_PIN, _freq, 0.0);
	    _on = false;
	    _active = false;
	    return false;
	}

	bool exec() {
//		Serial.println("Feeder Exec");
		if (!_active) {
			return false;
		}
		if (millis() >= _nextTime) {
			// it's the next transition time
			Serial.print(millis());Serial.print(", ");Serial.println(_nextTime);
			if (_shotNumber >= _numberOfShots) {
				// last shot is done
				Serial.println("Burst Done");
				_pwm->setPWM(PWM_PIN, _freq, 0.0);
				_on = false;
				_shotNumber = 0;
				_nextTime = 0;
				_active = false;
				return true;
			}
			//// TODO adjust feed rate based on burstRate
			//// TODO deal with the case where off-time is (near?) zero
			if (_on) {
				// feeder is on, so turn it off and set the next transition time
				_pwm->setPWM(PWM_PIN, _freq, 0.0);
				_on = false;
				_nextTime = (((_shotInterval * _shotNumber) + MIN_FEED_INTERVAL) + _burstStartTime);
				Serial.print("On time: ");Serial.print(_nextTime);Serial.print(", ");
				Serial.println(((_shotInterval * _shotNumber) + MIN_FEED_INTERVAL));
			} else {
				// feeder is off, so turn it on and set the next transition time
				_dutyCycle = 90.0;  //// TMP TMP TMP
		        _pwm->setPWM(PWM_PIN, _freq, _dutyCycle);
		        _on = true;
				_nextTime = ((_shotInterval * (_shotNumber + 1)) + _burstStartTime);
				Serial.print("Off time: ");Serial.print(_nextTime);Serial.print(", ");
				Serial.println((_shotInterval * (_shotNumber + 1)));
			}
			_shotNumber++;
			Serial.print("Shot #: ");Serial.println(_shotNumber);
		}
		Serial.flush();
		return false;
	};

	bool burst(uint32_t numberOfShots, float shotRate) {
		Serial.println("Feeder Burst");
	    if (_burstSetup(numberOfShots, shotRate)) {
            return true;
        }
        if (!_primed) {
            prime(PRIME_PELLETS);
            _primed = true;
        }
        _active = true;

        //// FIXME figure out proper dutyCycle
		_dutyCycle = _normDutyCycle(_rateToDutyCycle(_burstRate));
		_dutyCycle = 90.0;  //// TMP TMP TMP
        _pwm->setPWM(PWM_PIN, _freq, _dutyCycle);
        _on = true;
		_nextTime = ((_shotInterval * (_shotNumber + 1)) + millis());
		Serial.print("First time: ");Serial.println(_nextTime);
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

	void setDutyCycle(float dutyCycle) { _dutyCycle = _normDutyCycle(dutyCycle); Serial.println(_dutyCycle); };

	float getDutyCycle() { return _dutyCycle; };

protected:
	float _freq = 2000;		//// FIXME 
	float _dutyCycle = 0.0;
	bool _primed = false;
	bool _on = false;
	unsigned long _nextTime = 0;

	RP2040_PWM *_pwm;
};
