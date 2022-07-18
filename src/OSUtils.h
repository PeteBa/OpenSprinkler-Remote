// OSUtils.h

#ifndef _OSUtils_h
#define _OSUtils_h

#include <Arduino.h>

time_t now();
void set_now(time_t time);

//*******************************************************************************/
//* Debug Print
//*******************************************************************************/

#define DEBUG_LEVEL_ERROR	0
#define DEBUG_LEVEL_WARN	DEBUG_LEVEL_ERROR + 1
#define DEBUG_LEVEL_INFO	DEBUG_LEVEL_WARN + 1
#define DEBUG_LEVEL_TRACE	DEBUG_LEVEL_INFO + 1

#define DEBUG_LEVEL DEBUG_LEVEL_INFO

#define DEBUG_TIMESTAMP {uint32 t = now(); Serial.printf("%02d %02d:%02d:%02d %05d ", t/(60*60*24), t/(60*60)%24, (t/60)%60, t%60, ESP.getFreeHeap());}
#define DEBUG_OUTPUT(msg, ...) {Serial.printf(msg, ##__VA_ARGS__);}
#define DEBUG_TRACE(msg, ...) {if (DEBUG_LEVEL >= DEBUG_LEVEL_TRACE) {DEBUG_TIMESTAMP {Serial.printf("TRACE: %s\n", msg);}}}
#define DEBUG_INFO(msg, ...) {if (DEBUG_LEVEL >= DEBUG_LEVEL_INFO) {DEBUG_TIMESTAMP {Serial.printf("INFO: " msg, ##__VA_ARGS__);}}}
#define DEBUG_WARN(msg, ...) {if (DEBUG_LEVEL >= DEBUG_LEVEL_WARN) {DEBUG_TIMESTAMP {Serial.printf("WARN: " msg, ##__VA_ARGS__);}}}
#define DEBUG_ERROR(msg, ...) {if (DEBUG_LEVEL >= DEBUG_LEVEL_ERROR) {DEBUG_TIMESTAMP {Serial.printf("ERROR: " msg, ##__VA_ARGS__);}}}

//*******************************************************************************/
//* OSTimer
//*******************************************************************************/

#define TIMER_CLEAR	0
#define TIMER_SET	1

class OSTimer {
private:
	time_t _alarm = 0;
	bool _isSet = false;
public:
	void set(int duration);
	void clear();
	bool isSet();
	bool isTriggered();
	int remaining();
};

//*******************************************************************************/
//* OSValve
//*******************************************************************************/

class OSValve {
private:
	int _control_pin;
	int _enable_pin;
	int _pulse_duration;
	bool _isOpen;
public:
	OSValve(int control_pin, int enable_pin, int pulse_duration);
	void open();
	void close();
	bool isOpen();
};

//*******************************************************************************/
//* OSPushover
//*******************************************************************************/

class OSPushover {
	const char * _token;
	const char * _user;
public:
	OSPushover(const char * token, const char * user);
	void send(const char * title, const char * message);
};
#endif
