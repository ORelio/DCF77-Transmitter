/* (c) 2018-2020 Luigi Calligaris. GPL v2+ */
/* (c) 2023-2024 ORelio - bug fixes and improvements */
/* Hardware Timer1 carrier generation for ~25.83 kHz (3rd harmonic ~77.5 kHz) */

#include <ESP8266WiFi.h>
#include "time.h"
#include "dcf77protocol.h"
// #include "dcf77protocol.c" // Uncomment in case compiler does not include it automatically

/* ==============
 *  BEGIN CONFIG
 * ============== */

#if __has_include("config.h")
  #include "config.h"
#else
  #error "Configuration file 'config.h' is missing! Please copy 'config.example.h' to 'config.h' and adjust it."
#endif

/* ============
 *  END CONFIG
 * ============ */

// Hardware Timer1 configuration for generating the 77.5 kHz carrier via its 3rd harmonic (~25.833 kHz).
// Software PWM (analogWrite) quantizes to ~26.2 kHz (3rd harmonic ~78.6 kHz), missing the narrow DCF77 receiver filter.
// Theoretical value: 80,000,000 Hz / (1548 * 2) = 25,839.79 Hz (3rd harmonic: 77,519.38 Hz).
// Measured output (accounting for crystal tolerance & ISR latency): ~25,822 Hz (3rd harmonic: ~77,466 Hz).
const uint32_t TIMER_TICKS = 1548;
volatile bool carrier_enabled = false;


// Global variables initialized during setup()
static uint8_t dcf77_data[DCF77_DATALEN];
static int ntp_sync_hour = 0;
static int ntp_sync_minute = 0;
static int ntp_sync_second = 0;
static int tx_minutes_remaining = 0;

// Timer1 Interrupt Service Routine (runs in IRAM for maximum speed and stability)
void IRAM_ATTR onTimerISR()
{
	if (carrier_enabled)
	{
		GPO ^= (1 << pwm_pin);  // Toggle antenna pin
	}
	else
	{
		GPOC = (1 << pwm_pin);  // Force antenna pin LOW when carrier is muted
	}
}

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

void DeepSleepCorrected(uint64_t delay_microseconds, float correction_factor)
{
	// Adjust deep sleep delay using expected internal clock drift factor. See formula in config.
	uint64_t delay_corrected = delay_microseconds + delay_microseconds * sleep_clock_correction;
	int sleep_minutes = delay_microseconds / 60000000;
	int sleep_seconds = (delay_microseconds % 60000000) / 1000000;
	Serial.printf("Sleeping for %d:%d minutes. Correction factor: %02.2f. Sleep delay: %lu µs", sleep_minutes, sleep_seconds, correction_factor, (long unsigned)delay_corrected);
	Serial.println();
	ESP.deepSleep(delay_corrected); // ESP.deepSleep() never returns, need to wire D0 to RST so that ESP resets itself after specified delay.
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
		delay(1000);
		Serial.print(".");
		if (++delay_retries == 30) {
			Serial.println(" Timeout");
			if (intermittent_mode)
			{
				// Give up for this hour to save power, adjusting sleep delay by deducting time since boot.
				Serial.println("Failed to connect to Wi-Fi access point, retrying next hour.");
				DeepSleepCorrected((uint64_t)3600000000 - system_get_time(), sleep_clock_correction);
			}
			else
			{
				Serial.println("Failed to connect to Wi-Fi access point, reseting.");
				ESP.reset(); // Give up after 30 seconds and hard reset the ESP to make sure the Wi-Fi chip will reboot as well
			}
		}
	}
	Serial.println(" Connected");

	Serial.print("Syncing with ");
	Serial.println(ntp_server);
	delay_retries = 0;
	configTime(time_zone, ntp_server); // https://github.com/esp8266/Arduino/blob/master/cores/esp8266/time.cpp

	// Wait for NTP sync to complete
	do
	{
		delay(1000);
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		PrintLocalTime();
		if (++delay_retries >= 30) {
			Serial.println("Timeout");
			if (intermittent_mode) {
				Serial.println("Failed to sync with NTP server, retrying next hour.");
				DeepSleepCorrected((uint64_t)3600000000 - system_get_time(), sleep_clock_correction);
			} else {
				Serial.println("Failed to sync with NTP server, restarting.");
				ESP.restart();
			}
		}
	}
	while (timeinfo->tm_year == 70); // January 1st 1970 means not synced yet (unix epoch time); tm_year contains years since 1900, hence 70

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
	pinMode(pwm_pin, OUTPUT);
	digitalWrite(pwm_pin, LOW);
	pinMode(led_pin, OUTPUT);

	// Sync time over Wi-Fi first before starting the high-frequency timer
	struct tm *timeinfo = NtpSync();

	// Initialize Hardware Timer1 for carrier generation
	timer1_isr_init();
	timer1_attachInterrupt(onTimerISR);
	timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
	timer1_write(TIMER_TICKS);

	if (intermittent_mode)
	{
		tx_minutes_remaining = tx_duration_minutes;
		Serial.printf("Intermittent mode: Transmitting for %d minutes, then every hour around :%02d for %d minutes.", tx_duration_minutes, tx_minute_start, tx_duration_minutes);
		Serial.println();
	}
	else
	{
		Serial.println("Continuous mode: Transmitting all time long, except when syncing with NTP server.");
		ntp_sync_hour = (timeinfo->tm_hour + ntp_sync_interval_hours) % 24;
		ntp_sync_minute = timeinfo->tm_min % 40 + 10; // Always sync somewhere between 10 and 50, transmission must be stable around o'clock
		ntp_sync_second = timeinfo->tm_sec;
		Serial.printf("NTP will resync at %02d:%02d:%02d", ntp_sync_hour, ntp_sync_minute, ntp_sync_second);
		Serial.println();
	}

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
	if (timeinfo->tm_sec < DCF77_DATALEN)
	{
		// Transmit bit (0 or 1)
		SwitchLed(1);
		carrier_enabled = false; // Stop carrier signal for bit modulation
		delayMicroseconds(dcf77_data[timeinfo->tm_sec] == 0 ? 100000 : 200000); // 100 ms = 0, 200 ms = 1
		SwitchLed(0);
		carrier_enabled = true;  // Resume carrier signal
	}
	else
	{
		// Last bit (59th bit) is not transmitted at all -not 0 nor 1- so the signal stays ON
		carrier_enabled = true;
	}

	// Status info over serial
	Serial.print("Transmitted bit ");
	if (timeinfo->tm_sec >= DCF77_DATALEN)
		Serial.print("-");
	else
		Serial.printf("%d", dcf77_data[timeinfo->tm_sec]);
	Serial.print(" @ ");
	PrintLocalTime();

	// Refresh data array every minute
	if (timeinfo->tm_sec == 0)
	{
		// At this point, we just retransmitted bit 0 (start of transmission) from previous payload. This bit is always 0.
		// We are now at second 0, dcf77_encode_data() can correctly compute next minute (payload time is one minute ahead).
		Serial.print("Calculating next DCF time code: ");
		Serial.println(dcf77_encode_data(timeinfo, dcf77_data));
	}

	// Continuous mode (default): Resync with NTP server when reaching next resync time. Resync time is calculated at startup.
	if (!intermittent_mode && timeinfo->tm_hour == ntp_sync_hour && timeinfo->tm_min == ntp_sync_minute && timeinfo->tm_sec == ntp_sync_second)
	{
		// Hard-Reset ESP to relaunch sync.
		// Just calling NtpSync() again could cause weird bugs such as issues with Wi-Fi or internal clock
		timer1_disable();
		ESP.reset();
	}

	// Intermittent mode: Put ESP to deep sleep at end of transmission, then wake up every hour at the same minute for transmitting again.
	if (intermittent_mode && timeinfo->tm_sec == 0 && --tx_minutes_remaining == 0)
	{
		timer1_disable();
		int sleep_minutes = tx_minute_start - timeinfo->tm_min;
		int wakeup_hour = timeinfo->tm_hour;
		if (sleep_minutes < 0) {
			// Fix values when tx start and tx end happen the same hour instead of overlapping around o'clock
			sleep_minutes += 60;
			wakeup_hour = (wakeup_hour + 1) % 24;
		}
		uint64_t delay_microseconds = (uint64_t)sleep_minutes * 60000000 - 30000000; // 30 seconds before tx_minute_start (need to initialize Wi-Fi and sync with NTP server)
		Serial.printf("Sleeping until %02d:%02d:30 +/- clock drift.", wakeup_hour, tx_minute_start);
		Serial.println();
		DeepSleepCorrected(delay_microseconds, sleep_clock_correction);
	}

	// Wait for next second
	gettimeofday(&tv, NULL);
	delayMicroseconds(1000000 - tv.tv_usec);
}
