/******************************************************************************
 * 
 * AutoTurret Feeder headers file
 * 
 ******************************************************************************/

    case FEED:
      dutyCycle = normDutyCycle(cmd.data.dutyCycle);
      PWM_Instance->setPWM(PWM_PIN, freq, dutyCycle);
      break;


class FeederController : public TimedController {
public:
	FeederController(BurstState_t *burstPtr) {
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

	bool exec() {
		Serial.println("Feeder Exec");
		return true;
	};

	void prime(uint16_t numPellets) {
		Serial.print("Feeder Prime: ");Serial.print(numPellets);Serial.print(", ");
	    _pwm->setPWM(PWM_PIN, freq, MAX_DUTY_CYCLE);
	    delay(numPellets * MAX_FEED_RATE);
		Serial.println(numPellets * MAX_FEED_RATE);
	    _pwm->setPWM(PWM_PIN, freq, 0.0);
	    _primed = true;
	}

	// try to unjam the feeder by going back and forth a bit
	void clear() {
	    Serial.println("Feeder Clear");
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

	void feed(float dutyCycle) {
		Serial.print("Feed @ ");Serial.print(dutyCycle);Serial.println("%");
	}

	void start(uint32_t pellets, float shotRate) {
		Serial.println("Feeder Start");
	    unsigned long now = millis();

		//// FIXME select proper speed to do that many shots in the given time
	    uint32_t duration = ?(shotRate);
	    duration = ?

		float dutyCycle = MAX_DUTY_CYCLE;
//	    dutyCycle = rateToDutyCycle(shotRate);
	    dutyCycle = normDutyCycle(dutyCycle);
	    _pwm->setPWM(PWM_PIN, freq, dutyCycle);

	    feederTime = now + duration;
	}

	void stop() {
	  Serial.println("Feeder Stop");
	  _pwm->setPWM(PWM_PIN, freq, 0.0);
	  feederTime = 0;
	}

	bool isOn() { return _feederOn; };

	bool isPrimed() { return _primed; };

	void setFrequency(float freq) { _freq = freq; };

	float getFrequency() { return _freq; };
protected:
	bool _feederOn = false;
	bool _primed = false;
	float _freq = 2000;		//// FIXME 

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
