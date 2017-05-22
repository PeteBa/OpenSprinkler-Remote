#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <Time.h>
#include "OSUtils.h"

//*******************************************************************************/
//* Global variables
//*******************************************************************************/

ESP8266WebServer * myServer = NULL;
OSNTPServer * myNTPServer = NULL;
OSValve * myValve = NULL;
OSTimer * myTimer = NULL;
OSConfig * myConfig = NULL;

//*******************************************************************************/
//* Prototypes
//*******************************************************************************/

void handleRoot();
void handleSetValve();
void handleGetOptions();
void handleNotFound();

void handleNTPUpdate();

void handleOTAStart();
void handleOTAEnd();
void handleOTAProgress(unsigned int progress, unsigned int total);
void handleOTAError(ota_error_t error);

void sendPushNotification(const char * message);

//*******************************************************************************/
//* Setup
//*******************************************************************************/

void setup(void)
{
	Serial.begin(115200);
	WiFi.printDiag(Serial);

	DEBUG_TRACE(__PRETTY_FUNCTION__);

	DEBUG_INFO("Reset reason: %s\n", ESP.getResetReason().c_str());
	DEBUG_INFO("SDK Version: %s\n", ESP.getSdkVersion());
	DEBUG_INFO("Free heap size:%d\n", ESP.getFreeHeap());
	DEBUG_INFO("Free sketch space:%d\n", ESP.getFreeSketchSpace());

	myValve = new OSValve();
	myValve->setup();

	myTimer = new OSTimer();
	myTimer->clear();

	DEBUG_INFO("Opening configuration portal for %d seconds\n", CONFIG_PORTAL_TIMEOUT);

	myConfig = new OSConfig(CONFIG_FILE);
	myConfig->load();
	myConfig->showPortal(CONFIG_PORTAL_TIMEOUT);
	myConfig->save();

	DEBUG_INFO("Closed configuration portal\n");

	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		DEBUG_INFO("Failed to connect before timeout\n");
		DEBUG_INFO("Resetting device\n");
		ESP.reset();
		delay(5000);
	}

	IPAddress myIP = WiFi.localIP();
	DEBUG_INFO("Connected with SSID = %s, IP = %d.%d.%d.%d\n", WiFi.SSID().c_str(), myIP[0], myIP[1], myIP[2], myIP[3]);
	sendPushNotification("OpenSprinkler Remote has restarted/n");

	myServer = new ESP8266WebServer(80);
	myServer->on("/", handleRoot);
	myServer->on("/cm", handleSetValve);
	myServer->on("/_cm", handleSetValve);
	myServer->on("/jo", handleGetOptions);
	myServer->onNotFound(handleNotFound);
	myServer->begin();

	myNTPServer = new OSNTPServer();
	myNTPServer->onUpdate(handleNTPUpdate);
	myNTPServer->startSync();

	ArduinoOTA.onStart(handleOTAStart);
	ArduinoOTA.onEnd(handleOTAEnd);
	ArduinoOTA.onProgress(handleOTAProgress);
	ArduinoOTA.onError(handleOTAError);
	ArduinoOTA.begin();

	pinMode(PIN_WATER_LEVEL, INPUT_PULLUP);

	DEBUG_INFO("Server started\n");
	delay(5000);
}

//*******************************************************************************/
//* Loop
//*******************************************************************************/

void loop(void) {
	static time_t lastCheckedWater = 0;
	static time_t lastSyncedNTP = 0;

	myServer->handleClient();

	if (myTimer->isSet() && myTimer->isTriggered()) {
		DEBUG_INFO("Timer triggered\n");
		if (myValve->status() != VALVE_CLOSED) {
			myValve->close();
			sendPushNotification("Valve closed by timer");
		}
		myTimer->clear();
	}

	if (now() - lastCheckedWater > 24 * 60 * 60) {
		if (digitalRead(PIN_WATER_LEVEL) == LOW) {
			DEBUG_INFO("Water level low\n");
			sendPushNotification("Water level low");
			lastCheckedWater = now();
		}
	}

	if (myNTPServer->isSyncing()) {
		myNTPServer->handleSync();
		lastSyncedNTP = now();
	}
	else {
		if (now() - lastSyncedNTP > 24 * 60 * 60) {
			myNTPServer->startSync();
			lastSyncedNTP = now();
		}
	}

	ArduinoOTA.handle();
}

//*******************************************************************************/
//* Push Notifications
//*******************************************************************************/

void sendPushNotification(const char * message) {
	WiFiClient myClient;

	DEBUG_TRACE(__PRETTY_FUNCTION__);

	if (strlen(myConfig->getPushoverToken()) > 0 && strlen(myConfig->getPushoverUser()) > 0) {

		DEBUG_INFO("Sending PushOver notification = %s\n", message);

		if (myClient.connect(PUSHOVER_URL, PUSHOVER_PORT)) {
			String data = \
				"token=" + String(myConfig->getPushoverToken()) + \
				"&user=" + String(myConfig->getPushoverUser()) + \
				"&title=" + String(myConfig->getPushoverTitle()) + \
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
			delay(200);

			DEBUG_INFO("Pushover response = ");
			while (myClient.available()) {
				char c = myClient.read();
				DEBUG_OUTPUT("%c", c);
				if (c == '\n') break;
			}

			myClient.stop();
		}
	}
}

//*******************************************************************************/
//* OTA Handlers
//*******************************************************************************/

void handleOTAStart() {
	DEBUG_INFO("OTA Start\n");
}

void handleOTAEnd() {
	DEBUG_INFO("OTA End\n");
}

void handleOTAProgress(unsigned int progress, unsigned int total) {
	DEBUG_INFO("OTA Progress: %u%%\r", (progress / (total / 100)));
}

void handleOTAError(ota_error_t error) {
	DEBUG_ERROR("OTA Error[%u]: ", error);

	if (error == OTA_AUTH_ERROR) {
		DEBUG_OUTPUT("Auth Failed\n");
	}
	else if (error == OTA_BEGIN_ERROR) {
		DEBUG_OUTPUT("Begin Failed\n");
	}
	else if (error == OTA_CONNECT_ERROR) {
		DEBUG_OUTPUT("Connect Failed\n");
	}
	else if (error == OTA_RECEIVE_ERROR) {
		DEBUG_OUTPUT("Receive Failed\n");
	}
	else if (error == OTA_END_ERROR) {
		DEBUG_OUTPUT("End Failed\n");
	}
}

//*******************************************************************************/
//* NTP Handlers
//*******************************************************************************/

void handleNTPUpdate() {
	uint32 ntp_clock;
	time_t local_clock;
	int diff;

	DEBUG_TRACE(__PRETTY_FUNCTION__);

	local_clock = now();
	ntp_clock = myNTPServer->getCurrentTimestamp();
	diff = ntp_clock - local_clock;

	if (diff != 0) {
		adjustTime(diff);
		DEBUG_INFO("Clock sync'ed with NTP server (diff = %ds).\n", diff);
	}
}

//*******************************************************************************/
//* Server Handlers
//*******************************************************************************/

// Command: GET /
//
// Response: Display hhtp page showing valve status.
void handleRoot() {
	bool state;
	const char * label;
	const char * colour;

	DEBUG_TRACE(__PRETTY_FUNCTION__);

	state = myValve->status();
	label = (state == VALVE_CLOSED) ? "Closed" : "Open";
	colour = (state == VALVE_CLOSED) ? "Red" : "Green";

	String button = "<html>" \
						"<head>" \
							"<meta http-equiv='refresh' content='5'/>" \
							"<title>Remote Station</title>" \
						"</head>" \
						"<body>" \
							"<script>" \
								"var valve_status=" + String(state) + ";" \
								"function sf(valve) {" \
									"_cm.elements[0].value=valve;" \
									"_cm.elements[1].value=1-valve_status;" \
									"_cm.elements[2].value=" + String(TIMER_DEFAULT_DURATION) + ";" \
									"_cm.submit()" \
								"}" \
							"</script>" \
							"<form name=_cm action=_cm method=get>" \
								"<input type=hidden name=sid>" \
								"<input type=hidden name=en>" \
								"<input type=hidden name=t>" \
							"</form>" \
							"<input type=button value='Station 0 - " + String(label) + " Timer - " + String(myTimer->remaining()) + "' id=s0 " \
							"style='white-space:normal;width:200px;height:100px;font-size:20px;background-color:" + String(colour) + "' " \
							"onClick=sf(0)>" \
						"</body>" \
					"</html>";

	myServer->send(200, "text/html", button);
}

// Command: GET /cm?pw=xxx&sid=x&en=x&t=x	(OpenSprinkler command)
// Command: GET /_cm?pw=xxx&sid=x&en=x&t=x	(WebServer command)
//
// Response: Set valve for specified time (and refresh page if from the WebServer).
void handleSetValve() {

	DEBUG_TRACE(__PRETTY_FUNCTION__);

	unsigned int sid = myServer->arg("sid").toInt();
	unsigned int state = myServer->arg("en").toInt();
	unsigned int duration = myServer->arg("t").toInt();

	DEBUG_INFO("Handling Set Valve request: SID = %d, State = %d, Duration = %d\n", sid, state, duration);

	if ((sid != 0) || (state > 1) || (duration > 600)) {
		DEBUG_ERROR("Received badly formed command: GET %s sid=%d en=%d t=%d",
			myServer->uri().c_str(), sid, state, duration);
		return;
	}

	if (state == 1) {
		if (myValve->status() != VALVE_OPEN) {
			myValve->open();
			sendPushNotification("Valve opened");
		}
		myTimer->set(duration);
	}
	else {
		if (myValve->status() != VALVE_CLOSED) {
			myValve->close();
			sendPushNotification("Valve closed");
		}
		if (myTimer->isSet())
			myTimer->clear();
	}
	if (myServer->uri() == "/_cm") // Update the webserver page to reflect valve status
		myServer->send(200, "text/html", "<script>window.location=\"/\";</script>\n");
}

// Get Options:
//		GET /jo?pw=a6d82bced638de3def1e9bbb4983225c&_= (OpenSprinkler)
void handleGetOptions() {

	DEBUG_TRACE(__PRETTY_FUNCTION__);
	DEBUG_INFO("Handling Get Options request\n");

	myServer->send(200, "text/json", "{\"fwv\":216,\"re\":1}");
}

void handleNotFound() {

	DEBUG_TRACE(__PRETTY_FUNCTION__);

	String message = \
		"File Not Found\n\n" \
		"URI: " + myServer->uri() + "\n" \
		"Method: " + (myServer->method() == HTTP_GET) ? "GET\n" : "POST\n" \
		"Arguments: " + String(myServer->args()) + "\n";

	for (uint8_t i = 0; i < myServer->args(); i++) {
		message += " " + myServer->argName(i) + ": " + myServer->arg(i) + "\n";
	}

	DEBUG_WARN("Handling Unrecognised request: %s\n", message.c_str());
	myServer->send(404, "text/plain", message);
}
