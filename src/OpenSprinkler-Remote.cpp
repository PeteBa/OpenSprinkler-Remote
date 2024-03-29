#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "OSUtils.h"
#include "OSConfig.h"

#define MAX_CYCLE_TIME	1000	// Opensprinkler refreshes remote station status every 800 seconds
								// See Opensprinker github (OpenSprinkler.cpp) for how this is calculated

//*******************************************************************************/
//* Global variables
//*******************************************************************************/

ESP8266WebServer * myServer = NULL;
OSPushover * myNotifier = NULL;
OSValve * myValve = NULL;
OSTimer * myTimer = NULL;

//*******************************************************************************/
//* Prototypes
//*******************************************************************************/

void handleRoot();
void handleSetValve();
void handleGetOptions();
void handleNotFound();

//*******************************************************************************/
//* Setup
//*******************************************************************************/

void setup(void)
{
	Serial.begin(115200);

	DEBUG_TRACE(__PRETTY_FUNCTION__);

	DEBUG_INFO("Reset reason: %s\n", ESP.getResetReason().c_str());
	DEBUG_INFO("SDK Version: %s\n", ESP.getSdkVersion());
	DEBUG_INFO("Free heap size:%d\n", ESP.getFreeHeap());
	DEBUG_INFO("Free sketch space:%d\n", ESP.getFreeSketchSpace());

#ifdef PUSHOVER_ENABLED
	myNotifier = new OSPushover(PUSHOVER_TOKEN, PUSHOVER_USER);
#endif
	myValve = new OSValve(VALVE_CONTROL_PIN, VALVE_ENABLE_PIN, VALVE_PULSE_DURATION);
	myTimer = new OSTimer();

	DEBUG_INFO("Connecting to SSID = %s\n", MY_SSID);

	WiFi.hostname(HOSTNAME);
	WiFi.mode(WIFI_STA);
	WiFi.begin(MY_SSID, MY_PSK);
	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		DEBUG_ERROR("Failed to connect to %s before timeout\n", MY_SSID);
		delay(3000);
		ESP.reset();
		delay(5000);
	}

	IPAddress myIP = WiFi.localIP();
	DEBUG_INFO("Connected with SSID = %s, IP = %d.%d.%d.%d\n", WiFi.SSID().c_str(), myIP[0], myIP[1], myIP[2], myIP[3]);

	myServer = new ESP8266WebServer(80);
	myServer->on("/", handleRoot);
	myServer->on("/cm", handleSetValve);
	myServer->on("/_cm", handleSetValve);
	myServer->on("/jo", handleGetOptions);
	myServer->onNotFound(handleNotFound);
	myServer->begin();

	pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);

	DEBUG_INFO("Server started\n");
	delay(5000);
}

//*******************************************************************************/
//* Loop
//*******************************************************************************/

void loop(void) {
	static time_t lastCheckedWater = 0;

	myServer->handleClient();

	if (myTimer->isTriggered()) {
		DEBUG_INFO("Timer triggered\n");
		if (myValve->isOpen()) {
			myValve->close();
			if (myNotifier)
				myNotifier->send(HOSTNAME, "Valve closed by timer");
		}
		myTimer->clear();
	}

	if (now() - lastCheckedWater > 24 * 60 * 60) {
		if (digitalRead(WATER_LEVEL_PIN) == LOW) {
			DEBUG_INFO("Water level low\n");
			if (myNotifier)
				myNotifier->send(HOSTNAME, "Water level low");
			lastCheckedWater = now();
		}
	}
}

//*******************************************************************************/
//* Server Handlers
//*******************************************************************************/

// Command: GET /
//
// Response: Display hhtp page showing valve status.
void handleRoot() {
	bool isOpen;
	const char * label;
	const char * colour;

	DEBUG_TRACE(__PRETTY_FUNCTION__);

	isOpen = myValve->isOpen();
	label = (isOpen) ? "Open" : "Closed";
	colour = (isOpen) ? "Green" : "Red";

	String button = "<html>" \
						"<head>" \
							"<meta http-equiv='refresh' content='5'/>" \
							"<title>Remote Station</title>" \
						"</head>" \
						"<body>" \
							"<script>" \
								"var valve_status=" + String(isOpen) + ";" \
								"function sf(valve) {" \
									"_cm.elements[0].value=valve;" \
									"_cm.elements[1].value=1-valve_status;" \
									"_cm.elements[2].value=120;" \
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

	int sid = myServer->arg("sid").toInt();
	int state = myServer->arg("en").toInt();
	int duration = myServer->arg("t").toInt();

	DEBUG_INFO("Handling Set Valve request: SID = %d, State = %d, Duration = %d\n", sid, state, duration);

	if ((sid != 0) || (state > 1)) {
		DEBUG_ERROR("Received badly formed command: GET %s sid=%d en=%d t=%d",
					myServer->uri().c_str(), sid, state, duration);
		return;
	}

	if (duration > MAX_CYCLE_TIME) {
		DEBUG_ERROR("Received duration is too long (%d seconds)\n", duration);
		DEBUG_ERROR("Check \"Remote Station Auto-Refresh\" option is enabled in OS Advanced Settings\n");
		return;
	}

	if (state == 1) {
		if (!myValve->isOpen()) {
			myValve->open();
			if (myNotifier)
				myNotifier->send(HOSTNAME, "Valve opened");
		}
		myTimer->set(duration);
	}
	else {
		if (myValve->isOpen()) {
			myValve->close();
			if (myNotifier)
				myNotifier->send(HOSTNAME, "Valve closed");
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
