#include <FS.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

extern "C" {
#include <sntp.h>
}

#include "OSUtils.h"

//*******************************************************************************/
//* OSTimer
//*******************************************************************************/

void OSTimer::set(unsigned int duration) {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	if (duration > TIMER_MAX_DURATION) {
		DEBUG_ERROR("Timer ignored as %d is greater than allowed duration\n", duration);
		return;
	}
	_alarm = millis() / 1000 + duration;
	_status = TIMER_SET;
}

void OSTimer::clear() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);
	_alarm = 0;
	_status = TIMER_CLEAR;
}

// Helper macro to determine if time is after alarm (handles overflow)
#define timeAfter(a,b)    (((int)(a) - (int)(b)) > 0)

bool OSTimer::isTriggered() {
	if (this->isSet() && timeAfter(millis() / 1000, _alarm)) {
		return true;
	}

	return false;
}

int OSTimer::remaining() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);
	if (this->isSet()) {
		return _alarm - millis() / 1000;
	}

	return 0;
}

//*******************************************************************************/
//* OSValve
//*******************************************************************************/

#define VALVE_CLOSE				VALVE_CLOSED

void OSValve::setup() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	pinMode(PIN_VALVE_A_CONTROL, OUTPUT);
	pinMode(PIN_VALVE_A_ENABLE, OUTPUT);

	digitalWrite(PIN_VALVE_A_CONTROL, VALVE_CLOSE);
	digitalWrite(PIN_VALVE_A_ENABLE, LOW);

	this->close();
}

void OSValve::open() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	digitalWrite(PIN_VALVE_A_CONTROL, VALVE_OPEN);
	digitalWrite(PIN_VALVE_A_ENABLE, HIGH);
	delay(VALVE_PULSE_DURATION);
	digitalWrite(PIN_VALVE_A_ENABLE, LOW);

	_status = VALVE_OPEN;

	DEBUG_INFO("Valve opened\n");
}

void OSValve::close() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	digitalWrite(PIN_VALVE_A_CONTROL, VALVE_CLOSE);
	digitalWrite(PIN_VALVE_A_ENABLE, HIGH);
	delay(VALVE_PULSE_DURATION);
	digitalWrite(PIN_VALVE_A_ENABLE, LOW);

	_status = VALVE_CLOSED;

	DEBUG_INFO("Valve closed\n");
}

//*******************************************************************************/
//* OSConfig
//*******************************************************************************/

static bool _isConfigModified = false;

void saveConfigCallback() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);
	_isConfigModified = true;
}

OSConfig::OSConfig(const char * configFile) {

	strncpy(_configFile, configFile, sizeof(_configFile));

	if (SPIFFS.begin()) {
		DEBUG_INFO("Mounting filesystem\n");
		_isSpiffsReady = true;
	}
	else {
		DEBUG_ERROR("Failed to mount filesystem\n");
	}
}

void OSConfig::showPortal(unsigned int duration) {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	WiFiManager wifiManager;

	wifiManager.setDebugOutput(false);

	WiFiManagerParameter custom_pushover_token("token", "Pushover Token", _pushoverToken, 31);
	WiFiManagerParameter custom_pushover_user("user", "Pushover User", _pushoverUser, 31);
	WiFiManagerParameter custom_pushover_title("title", "Message Title", _pushoverTitle, 31);

	wifiManager.addParameter(&custom_pushover_token);
	wifiManager.addParameter(&custom_pushover_user);
	wifiManager.addParameter(&custom_pushover_title);

	wifiManager.setConfigPortalTimeout(duration);
	wifiManager.setSaveConfigCallback(saveConfigCallback);

	_isConfigModified = false;
	if (!wifiManager.startConfigPortal(CONFIG_PORTAL_AP, CONFIG_PORTAL_PSK)) {
		DEBUG_INFO("Config portal failed to connect\n");
	}

	if (_isConfigModified) {
		strncpy(_pushoverToken, custom_pushover_token.getValue(), sizeof(_pushoverToken));
		strncpy(_pushoverUser, custom_pushover_user.getValue(), sizeof(_pushoverUser));
		strncpy(_pushoverTitle, custom_pushover_title.getValue(), sizeof(_pushoverTitle));
	}
}

void OSConfig::load() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	if (_isSpiffsReady && SPIFFS.exists(_configFile)) {
		DEBUG_INFO("Opening config file\n");

		File configFile = SPIFFS.open(_configFile, "r");

		if (configFile) {
			size_t size = configFile.size();
			std::unique_ptr<char[]> buf(new char[size]);
			configFile.readBytes(buf.get(), size);

			DynamicJsonBuffer jsonBuffer;
			JsonObject& json = jsonBuffer.parseObject(buf.get());

			if (json.success()) {
				strncpy(_pushoverToken, json["pushover_token"], sizeof(_pushoverToken));
				strncpy(_pushoverUser, json["pushover_user"], sizeof(_pushoverUser));
				strncpy(_pushoverTitle, json["pushover_title"], sizeof(_pushoverTitle));
				DEBUG_INFO("Read config: Token = %s, User = %s, Title = %s\n", _pushoverToken, _pushoverUser, _pushoverTitle);
			}
			else {
				DEBUG_ERROR("Failed to load json config\n");
			}

			configFile.close();
		}
		else {
			DEBUG_ERROR("Unable to open %s file\n", _configFile);
		}
	}
}

void OSConfig::save() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	if (_isSpiffsReady && _isConfigModified) {
		DEBUG_INFO("Saving configuration\n");

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		json["pushover_token"] = _pushoverToken;
		json["pushover_user"] = _pushoverUser;
		json["pushover_title"] = _pushoverTitle;

		File configFile = SPIFFS.open(_configFile, "w");
		if (configFile) {
			json.printTo(configFile);
			configFile.close();
			DEBUG_INFO("Written config: Token = %s, User = %s, Title = %s\n", _pushoverToken, _pushoverUser, _pushoverTitle);
		}
		else {
			DEBUG_INFO("Failed to open config file for writing\n");
		}
	}
	_isConfigModified = false;
}

void OSConfig::reset()
{
	DEBUG_TRACE(__PRETTY_FUNCTION__);

	if (_isSpiffsReady && SPIFFS.exists(_configFile)) {
		DEBUG_INFO("Resetting config file\n");
		SPIFFS.remove(_configFile);
	}
	_pushoverToken[0] = 0;
	_pushoverUser[0] = 0;
	_pushoverTitle[0] = 0;
}

//*******************************************************************************/
//* OSNTPServer
//*******************************************************************************/

const char * const OSNTPServer::_timeServers[] = NTP_SERVERS;

OSNTPServer::OSNTPServer() {

	for (int i = 0; i < sizeof(_timeServers)/sizeof(_timeServers[0]); i++)
		sntp_setservername(i, (char *)_timeServers[i]);

	sntp_set_timezone(NTP_LOCAL_OFFSET);

	_cb = NULL;
	_isSyncing = false;
}

void OSNTPServer::startSync() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);
	sntp_init();
	_isSyncing = true;
}

void OSNTPServer::stopSync() {
	DEBUG_TRACE(__PRETTY_FUNCTION__);
	sntp_stop();
	_isSyncing = false;
}

void OSNTPServer::handleSync() {

	if (this->isSyncing()) {
		if (this->getCurrentTimestamp() != 0) {
			if (_cb != NULL)
				(*_cb)();
			this->stopSync();
		}
	}
}

void OSNTPServer::onUpdate(void(*cb)(void)) {
	_cb = cb;
}

uint32 OSNTPServer::getCurrentTimestamp() {
	return sntp_get_current_timestamp();
}
