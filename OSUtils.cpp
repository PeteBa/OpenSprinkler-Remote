#include "OSUtils.h"

//*******************************************************************************/
//* Time Helpers
//*******************************************************************************/

static time_t _sysTime = 0;
static unsigned long _prevMillis = 0;

void set_now(time_t time) {
	_sysTime = time;
	_prevMillis = millis();
}

time_t now() {
	while (millis() - _prevMillis >= 1000) {
		_sysTime++;
		_prevMillis += 1000;	
	}
	return _sysTime;
}

//*******************************************************************************/
//* OSTimer
//*******************************************************************************/

void OSTimer::set(int duration) {
	DEBUG_TRACE(__PRETTY_FUNCTION__);
	DEBUG_INFO("Timer set for %d seconds\n", duration);
	_alarm = now() + duration;
	_status = TIMER_SET;
}

void OSTimer::clear() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);
	DEBUG_INFO("Timer cleared\n");
	_alarm = 0;
	_status = TIMER_CLEAR;
}

bool OSTimer::isSet() {
	return _status;
}

bool OSTimer::hasTriggered() {
	if (this->isSet() && (_alarm < now())) {
		return true;
	}

	return false;
}

int OSTimer::remaining() {
	if (this->isSet() && (_alarm > now())) {
		return _alarm - now();
	}

	return -1;
}

//*******************************************************************************/
//* OSValve
//*******************************************************************************/

#define VALVE_A_CONTROL_PIN		0	// GPIO0 (D3 on ESP-12E with ESP Motor Chield)
#define VALVE_A_ENABLE_PIN		5	// GPIO5 (D1 on ESP-12E with ESP Motor Chield)

#define VALVE_PULSE_DURATION	50	// 50ms pulse to latching valve

#define VALVE_CLOSE				VALVE_CLOSED

void OSValve::setup() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	pinMode(VALVE_A_CONTROL_PIN, OUTPUT);
	pinMode(VALVE_A_ENABLE_PIN, OUTPUT);

	digitalWrite(VALVE_A_CONTROL_PIN, VALVE_CLOSE);
	digitalWrite(VALVE_A_ENABLE_PIN, LOW);

	this->close();
}

void OSValve::open() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	digitalWrite(VALVE_A_CONTROL_PIN, VALVE_OPEN);
	digitalWrite(VALVE_A_ENABLE_PIN, HIGH);
	delay(VALVE_PULSE_DURATION);
	digitalWrite(VALVE_A_ENABLE_PIN, LOW);

	_status = VALVE_OPEN;

	DEBUG_INFO("Valve opened\n");
}

void OSValve::close() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	digitalWrite(VALVE_A_CONTROL_PIN, VALVE_CLOSE);
	digitalWrite(VALVE_A_ENABLE_PIN, HIGH);
	delay(VALVE_PULSE_DURATION);
	digitalWrite(VALVE_A_ENABLE_PIN, LOW);

	_status = VALVE_CLOSED;

	DEBUG_INFO("Valve closed\n");
}

bool OSValve::status() {
	return _status;
}
