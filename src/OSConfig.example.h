// Update and rename this file OSConfig.h

// Update WiFi details
#define MY_SSID			    "my_ssid"
#define MY_PSK			    "my_password"

// Uncomment if pushover.net account is available otherwise leave undefined
//#define PUSHOVER_ENABLED
#ifdef PUSHOVER_ENABLED
    #define PUSHOVER_TOKEN	    "my_pushover_token"
    #define PUSHOVER_USER	    "my_pushover_token"
#endif

// Valve details
#define VALVE_CONTROL_PIN		0	// GPIO0 (D3 on ESP-12E with ESP Motor Chield)
#define VALVE_ENABLE_PIN		5	// GPIO5 (D1 on ESP-12E with ESP Motor Chield)
#define VALVE_PULSE_DURATION	50	// 50ms pulse to latching valve

// Float switch pin
#define WATER_LEVEL_PIN	4

// Hostname
#define HOSTNAME		"my_remote_name"
