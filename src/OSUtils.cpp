#include <ESP8266WiFi.h>
#include "OSConfig.h"
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
	_isSet = true;
}

void OSTimer::clear() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);
	DEBUG_INFO("Timer cleared\n");
	_alarm = 0;
	_isSet = false;
}

bool OSTimer::isSet() {
	return _isSet;
}

bool OSTimer::isTriggered() {
	if (_isSet && (_alarm < now())) {
		return true;
	}

	return false;
}

int OSTimer::remaining() {
	time_t remain = _alarm - now();

	if (_isSet) {
		return remain > 0 ? remain : 0;
	}

	return -1;
}

//*******************************************************************************/
//* OSValve
//*******************************************************************************/

#define VALVE_OPEN	 1
#define VALVE_CLOSED 0

OSValve::OSValve(int control_pin, int enable_pin, int pulse_duration) :
	_control_pin(control_pin), _enable_pin(enable_pin), _pulse_duration(pulse_duration)
{
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	pinMode(_control_pin, OUTPUT);
	pinMode(_enable_pin, OUTPUT);

	digitalWrite(_control_pin, VALVE_CLOSED);
	digitalWrite(_enable_pin, LOW);

	this->close();
}

void OSValve::open() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	digitalWrite(_control_pin, VALVE_OPEN);
	digitalWrite(_enable_pin, HIGH);
	delay(_pulse_duration);
	digitalWrite(_enable_pin, LOW);

	_isOpen = true;

	DEBUG_INFO("Valve opened\n");
}

void OSValve::close() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	digitalWrite(_control_pin, VALVE_CLOSED);
	digitalWrite(_enable_pin, HIGH);
	delay(_pulse_duration);
	digitalWrite(_enable_pin, LOW);

	_isOpen = false;

	DEBUG_INFO("Valve closed\n");
}

bool OSValve::isOpen() {
	return _isOpen;
}


//*******************************************************************************/
//* OSPushover
//*******************************************************************************/
#define PUSHOVER_URL	"api.pushover.net"
#define PUSHOVER_PORT	80

OSPushover::OSPushover(const char * token, const char * user) :
	_token(token), _user(user)
{
	DEBUG_TRACE(__PRETTY_FUNCTION__);
}

void OSPushover::send(const char * title, const char * message) {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

#ifdef PUSHOVER_ENABLED

	WiFiClient myClient;

	if (_token != NULL && _user != NULL && title != NULL && message != NULL) {

		DEBUG_INFO("Sending PushOver notification = %s\n", message);

		if (myClient.connect(PUSHOVER_URL, PUSHOVER_PORT)) {
			String data = \
				"token=" + String(_token) + \
				"&user=" + String(_user) + \
				"&title=" + String(title) + \
				"&message=" + String(message);

			String msg = \
				"POST /1/messages.json HTTP/1.1\r\n" \
				"Host: " PUSHOVER_URL "\r\n" \
				"Connection: close\r\n" \
				"Content-Type: application/x-www-form-urlencoded;\r\n" \
				"Content-Length: " + String(data.length()) + "\r\n" \
				"\r\n" + \
				data + "\r\n";

			myClient.print(msg);
			delay(500);

			DEBUG_INFO("Pushover response = ");
			while (myClient.available()) {
				char c = myClient.read();
				if (c == '\n') break;
				DEBUG_OUTPUT("%c", c);
			}
			DEBUG_OUTPUT("\n");

			myClient.stop();
		}
	}
#endif
}
