// OSUtils.h

#ifndef _OSUtils_h
#define _OSUtils_h

#include <arduino.h>
#include <Time.h>
#include <TimeLib.h>

//*******************************************************************************/
//* Configuration Constants
//*******************************************************************************/

#define PIN_WATER_LEVEL			4				// GPIO pin connected to a water level sensor
#define PIN_VALVE_A_CONTROL		0				// GPIO pin to control forward/backward or +9V/-9V (D3 on ESP-12E with ESP Motor Chield)
#define PIN_VALVE_A_ENABLE		5				// GPIO pin to enable the H-Bridge and register the Control Pin (D1 on ESP-12E with ESP Motor Chield)

#define VALVE_PULSE_DURATION	50				// 50ms pulse to the the Enable Pin to switch the latching valve

#define TIMER_MAX_DURATION		60 * 60			// Limit the maximum duration to protect against overflowing
#define TIMER_DEFAULT_DURATION	120				// The User Interface button will triggers a default 120 second valve run

#define CONFIG_PORTAL_AP		"OSRemoteAP"	// SSID used for the Config Portal
#define CONFIG_PORTAL_PSK		"opendoor"		// Password for the Config Portal
#define CONFIG_PORTAL_TIMEOUT	180				// Duration, in seconds, allowed for the configuration sequence
#define CONFIG_FILE				"/config.json"	// Filename to store config information on spiffs drive

#define NTP_LOCAL_OFFSET		0				// Local timezone offset (i.e. 0 is UTC)
#define NTP_SERVERS				{ "us.pool.ntp.org","ntp.sjtu.edu.cn","time.microsoft.com" }

#define PUSHOVER_URL			"api.pushover.net"
#define PUSHOVER_PORT			80

//*******************************************************************************/
//* Debug Print
//*******************************************************************************/

#ifdef _DEBUG

#define DEBUG_LEVEL_NONE	0
#define DEBUG_LEVEL_ERROR	1
#define DEBUG_LEVEL_WARN	2
#define DEBUG_LEVEL_INFO	3
#define DEBUG_LEVEL_TRACE	4

#define DEBUG_LEVEL DEBUG_LEVEL_INFO

#define DEBUG_TIMESTAMP			{time_t t = now(); Serial.printf("%02d-%02d-%02d %02d:%02d:%02d %05d ", year(t), month(t), day(t), hour(t), minute(t), second(t), ESP.getFreeHeap());}
#define DEBUG_OUTPUT(msg, ...)	{Serial.printf(msg, ##__VA_ARGS__);}
#define DEBUG_TRACE(msg, ...)	{if (DEBUG_LEVEL >= DEBUG_LEVEL_TRACE) {DEBUG_TIMESTAMP {Serial.printf("TRACE: %s\n", msg);}}}
#define DEBUG_INFO(msg, ...)	{if (DEBUG_LEVEL >= DEBUG_LEVEL_INFO) {DEBUG_TIMESTAMP {Serial.printf("INFO: " msg, ##__VA_ARGS__);}}}
#define DEBUG_WARN(msg, ...)	{if (DEBUG_LEVEL >= DEBUG_LEVEL_WARN) {DEBUG_TIMESTAMP {Serial.printf("WARN: " msg, ##__VA_ARGS__);}}}
#define DEBUG_ERROR(msg, ...)	{if (DEBUG_LEVEL >= DEBUG_LEVEL_ERROR) {DEBUG_TIMESTAMP {Serial.printf("ERROR: " msg, ##__VA_ARGS__);}}}

#else

#define DEBUG_OUTPUT(msg, ...)	{}
#define DEBUG_TRACE(msg, ...)	{}
#define DEBUG_INFO(msg, ...)	{}
#define DEBUG_WARN(msg, ...)	{}
#define DEBUG_ERROR(msg, ...)	{}

#endif

//*******************************************************************************/
//* OSTimer
//*******************************************************************************/

#define TIMER_CLEAR	0
#define TIMER_SET	1

class OSTimer {
private:
	unsigned long _alarm = 0;
	bool _status = TIMER_CLEAR;
public:
	void set(unsigned int duration);
	void clear();
	inline bool isSet() { return _status; };
	bool isTriggered();
	int remaining();
};

//*******************************************************************************/
//* OSValve
//*******************************************************************************/

#define VALVE_OPEN	 1
#define VALVE_CLOSED 0

class OSValve {
private:
	bool _status;
public:
	void setup();
	void open();
	void close();
	inline bool status() { return _status; };
};

//*******************************************************************************/
//* OSConfig
//*******************************************************************************/

class OSConfig {
private:
	char _configFile[35] = { 0 };
	char _pushoverToken[35] = { 0 };
	char _pushoverUser[35] = { 0 };
	char _pushoverTitle[35] = { 0 };

	bool _isSpiffsReady = false;
public:
	OSConfig(const char * _configFile);
	void load();
	void save();
	void showPortal(unsigned int duration);
	void reset();
	inline const char * getPushoverToken() { return _pushoverToken; };
	inline const char * getPushoverUser() { return _pushoverUser; };
	inline const char * getPushoverTitle() { return _pushoverTitle; };
};

//*******************************************************************************/
//* OSNTPServer
//*******************************************************************************/

class OSNTPServer {
private:
	void(*_cb)(void) = NULL;
	bool _isSyncing = false;
	static const char * const _timeServers[];
public:
	OSNTPServer();
	void startSync();
	void stopSync();
	void handleSync();
	void onUpdate(void(*cb)(void));
	uint32 getCurrentTimestamp();
	inline bool isSyncing() { return _isSyncing; };
};

#endif
