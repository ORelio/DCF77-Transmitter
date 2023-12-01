/* (c) 2018-2020 Luigi Calligaris. GPL v2+ */
/* (c) 2023 ORelio - bug fixes and improvements */

// Code reused from the following repositories, mainly dcfake77 (ESP8266 version) with improvements from DCF77-Transmitter-for-ESP32
// https://github.com/luigicalligaris/dcfake77 - Base code and RF Transmission
// https://github.com/SensorsIot/DCF77-Transmitter-for-ESP32 - NTP with correct timezone and daylight saving time

#include <ESP8266WiFi.h>
#include "time.h"
#include "dcf77protocol.h"
// #include "dcf77protocol.c" // Uncomment in case compiler does not include it automatically

/* ==============
 *  BEGIN CONFIG
 * ============== */

// Wi-Fi configuration
//const char* wifi_ssid   = "MyWyFiNetwork";
//const char* wifi_pass   = "S3cr3tWiFiP4ssw0rd";

// NTP configuration
const char* ntp_server = "fr.pool.ntp.org"; // https://www.ntppool.org/use.html - https://www.ntppool.org/tos.html
const char* time_zone = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; // Pick a time zone from https://remotemonitoringsystems.ca/time-zone-abbreviations.php
const int ntp_sync_interval_hours = 6; // How many hours between NTP syncs over Wi-Fi. Transmitter temporarily stops when syncing over Wi-Fi.

// GPIO Pin configuration
const unsigned pwm_pin = 5;           // Antenna pin GPIO# - See https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
const unsigned led_pin = LED_BUILTIN; // LED pin GPIO# or use LED_BUILTIN - See https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
const bool led_enabled = true;        // Enable or disable built-in led. By default, flashes every second in transmitting mode, static on when syncing with NTP.

/* ============
 *  END CONFIG
 * ============ */

// Constants for generating DCF77 sin wave base signal (77.5 kHz). Users of this sketch should not need to edit this.
const unsigned pwm_freq       = 25833; // ESP8266 cannot directly generate 77500 Hz frequency. We use the 3rd harmonic of 25833 Hz to obtain 77.5 kHz
const unsigned pwm_resolution = 3;     // Analog wave resolution (in bits)
const unsigned pwm_duty_off   = 0;     // Analog wave amplitude in OFF state. Signal should be an attenuated in this state per specs, but OFF works just fine.
const unsigned pwm_duty_on    = 3;     // Analog wave amplitude in ON state

// Global variables initialized during setup()
const int dcf77_datalen = 59;
static uint8_t dcf77_data[dcf77_datalen];
static int ntp_sync_hour = 0;
static int ntp_sync_minute = 0;

void PrintLocalTime()
{
	time_t rawtime;
	time(&rawtime);
	Serial.print(ctime(&rawtime));
	Serial.print("\r");
}

void SwitchLed(bool on)
{
	if (on && led_enabled)
	{
		digitalWrite(led_pin, LOW);
	}
	else digitalWrite(led_pin, HIGH);
}

struct tm *NtpSync()
{
	time_t rawtime;
	struct tm * timeinfo;
	int delay_retries;

	Serial.println("Starting NTP Sync over Wi-Fi");
	SwitchLed(1);

	Serial.print("Connecting to ");
	Serial.print(wifi_ssid);
	WiFi.setHostname("DCF77GW");
	WiFi.begin(wifi_ssid, wifi_pass);
	delay_retries = 0;
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (++delay_retries == 60) {
			Serial.println(" Timeout");
			ESP.reset(); // Give up after 30 seconds and hard reset the ESP to make sure the Wi-Fi chip will reboot as well
		}
	}
	Serial.println(" Connected");

	Serial.print("Syncing with ");
	Serial.println(ntp_server);
	delay_retries = 0;
	do {
		configTime(time_zone, ntp_server); // https://github.com/esp8266/Arduino/blob/master/cores/esp8266/time.cpp
		delay(500);
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		PrintLocalTime();
		if (++delay_retries == 60) {
			Serial.println("Timeout");
			ESP.restart(); // Give up after 30 seconds and restart program (clean reboot)
		}
	} while (timeinfo->tm_year == 70 && timeinfo->tm_yday == 0); // january 1st 1970 means not synced yet

	Serial.print("Disconnecting from ");
	Serial.println(wifi_ssid);
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);

	SwitchLed(0);
	Serial.println("NTP Sync done");
	return timeinfo;
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Initializing NTP to DCF77 gateway");
	analogWriteFreq(pwm_freq);
	analogWriteRange(pwm_resolution);
	pinMode(led_pin, OUTPUT);

	struct tm * timeinfo = NtpSync();
	ntp_sync_hour = (timeinfo->tm_hour + ntp_sync_interval_hours) % 24;
	ntp_sync_minute = timeinfo->tm_min % 40 + 10; // Always sync between 10 and 50, transmission must be stable around o'clock
	Serial.printf("NTP will resync at %02d:%02d", ntp_sync_hour, ntp_sync_minute);
	Serial.println();

	Serial.print("Calculating current DCF time code: ");
	Serial.println(dcf77_encode_data(timeinfo, dcf77_data));
}

void loop()
{
	time_t rawtime;
	struct tm *timeinfo;
	struct timeval tv;
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	// DCF77 radio is ON by default (signal transmitted at 100% strength)
	// It transmits one bit per second by attenuating transmission (from 100% to 15%) for 100ms (bit 0) or 200ms (bit 1).
	// See https://en.wikipedia.org/wiki/DCF77#Amplitude_modulation
	// Bits are sent in sync with each second so the receiver can synchronize its clock with transmitter.
	if (timeinfo->tm_sec < dcf77_datalen)
	{
		// Transmit bit (0 or 1)
		SwitchLed(1);
		analogWrite(pwm_pin, pwm_duty_off); // Instead of attenuating signal we completely stop it. Seems to work fine.
		delayMicroseconds(dcf77_data[timeinfo->tm_sec] == 0 ? 100000 : 200000); //100 ms = 0, 200 ms = 1
		SwitchLed(0);
		analogWrite(pwm_pin, pwm_duty_on);
	}
	else
	{
		// Last bit (59th bit) is not transmitted at all -not 0 nor 1- so the signal stays ON
		analogWrite(pwm_pin, pwm_duty_on);
	}

	// Status info over serial
	Serial.print("Transmitted bit ");
	if (timeinfo->tm_sec >= dcf77_datalen)
		Serial.print("-");
	else
		Serial.printf("%d", dcf77_data[timeinfo->tm_sec]);
	Serial.print(" @ ");
	PrintLocalTime();

	// Resync with NTP server when reaching next resync time. Resync time is calculated at startup.
	if (timeinfo->tm_hour == ntp_sync_hour && timeinfo->tm_min == ntp_sync_minute)
	{
		// Hard-Reset ESP to relaunch sync.
		// Just calling NtpSync() again could cause weird bugs such as issues with Wi-Fi or internal clock
		ESP.reset();
	}

	// Refresh data array every minute
	if (timeinfo->tm_sec == 0)
	{
		// At this point, we just retransmitted bit 0 (start of transmission) from previous payload. This bit is always 0.
		// We are now at second 0, dcf77_encode_data() can correctly compute next minute (payload time is one minute ahead).
		Serial.print("Calculating next DCF time code: ");
		Serial.println(dcf77_encode_data(timeinfo, dcf77_data));
	}

	// Wait for next second before looping
	gettimeofday(&tv, NULL);
	delayMicroseconds(1000000-tv.tv_usec);
}
