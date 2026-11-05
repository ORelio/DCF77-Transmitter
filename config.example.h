#pragma once

/* ==============================================================
 *  USER CONFIGURATION
 * ============================================================== */


// This program allows syncing clocks using the DCF77 protocol in places where DCF77 is not available
// DCF77 is a German longwave time signal and standard-frequency radio station running in Europe at 77.5 kHz
// https://en.wikipedia.org/wiki/DCF77

// In order to use this program, you need an ESP8266-based microcontroller such as D1 Mini, some wire and a 330 Ohm resistor (recommended range: 330-500 Ohm).
// [Your Wi-Fi router] <-- (retrieves time: NTP protocol over Wi-Fi) <-- [ESP8266] --> (transmits time: DCF77 signal at 77.5 kHz) --> [clock]
// Wiring of the ESP8266 is as follows: GPIO PIN --- [330 Ohm] --- [COIL] --- GND PIN
// Make a coil with some wire. Start with a coil of ~10cm diameter with ~10 loops to generate a short-distance magnetic field (~10cm), then adjust to your needs.
// DISCLAIMER: Sending a stronger signal to cover a larger area is likely forbidden by your local laws. Do not use an actual antenna or send amplified signal without knowing what you are doing.
// NOTE: DCF77 signaling is slow. Each time code takes a whole minute to transmit. Devices should need at least 1-3 minutes to properly synchronize to the signal.

// Wi-Fi configuration
const char* wifi_ssid   = "MyWiFiNetwork";
const char* wifi_pass   = "S3cr3tWiFiP4ssw0rd";

// NTP configuration
const char* ntp_server = "pool.ntp.org"; // https://www.ntppool.org/use.html - https://www.ntppool.org/tos.html
const char* time_zone = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; // Pick a time zone from https://remotemonitoringsystems.ca/time-zone-abbreviations.php
const int ntp_sync_interval_hours = 3; // How many hours between NTP syncs over Wi-Fi. Transmitter temporarily stops when syncing over Wi-Fi. ESP's internal clock is imprecise, keep the value low.

// GPIO Pin configuration
const unsigned pwm_pin = 5;           // Antenna pin GPIO# - See https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
const unsigned led_pin = LED_BUILTIN; // LED pin GPIO# or use LED_BUILTIN - See https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
const bool led_enabled = true;        // Enable or disable built-in led. If enabled, the LED flashes in transmitting mode, and stays on when syncing with NTP over Wi-Fi.

// Intermittent mode - Optional, for advanced users
// ================================================
// Power saving mode for running on battery - Only transmit signal a few minutes per hour during the time span where the clock actually tries to synchronize with the time signal.
// Most clocks will try to synchronize every hour or every few hours a few minutes before o'clock. Adjust the values below to your needs.
// This mode uses ESP's deep sleep mode. Need to wire GPIO16 (D0) to RST for waking up from deep sleep, preferably using a resistor between 470-1K Ohm to avoid issues flashing the ESP.
// * Default implementation: Continuously transmit time, resync with NTP every few hours according to ntp_sync_interval_hours defined above. Compliant with Ntppool.org ToS.
// * Intermittent implementation: Only transmit time for a few minutes per hour, put ESP to deep sleep for the rest of the hour, then resync with NTP server and resume transmitting.
// Important note about time keeping during deep sleep - Clock drift
//  Putting the ESP into deep sleep mode will induce drift in its internal clock, so in this mode NTP sync happens *every hour* (on wakeup), regardless of sync interval set above.
//  The ESP *will* wake a few minutes too soon or too late due to clock drift, so adjust sleep_clock_correction to compensate, and define a transmission time span slightly larger than needed.
//  You should switch to a NTP service that allows syncing every hour at the same time (Ntppool.org does not allow this), or set up your own server - easy if you have a Linux system on your LAN.
//  Installing NTP server on raspbian (raspberry pi): "apt install ntp", then remove "limited" keyword in "restrict" directive in "/etc/ntp.conf", then "service ntp restart". Done.
// Relevant documentation:
//  https://nodemcu.readthedocs.io/en/dev/modules/rtctime/
//  https://www.ntppool.org/tos.html
//  https://vitux.com/how-to-setup-ntp-server-and-client-on-debian-11/
//  https://www.letscontrolit.com/wiki/index.php?title=Tutorial_Battery_Powered_Devices
//  https://www.esp8266.com/viewtopic.php?t=13101 // Problem flashing after RST and GPIO16 connected

// Intermittent mode configuration
const bool intermittent_mode = false;       // Set this to true to enable intermittent mode instead of continuous mode. Make sure to read caveats above.
const int tx_minute_start = 55;             // Start transmission on the specified minute. ESP *will* wake up too soon or too late, so adjust drift correction until getting satisfying results.
const int tx_duration_minutes = 10;         // Duration of transmission in minutes. After that, ESP will go to deep sleep to save power until next transmission.
const float sleep_clock_correction = 0.00f; // Set to 0.00f, run for a few hours. Note average sleep/wakeup time. correction = (desired_wakeup_time - actual_wakeup_time) / (actual_wakeup_time - sleep_start_time).
                                            // Example: Sleeping at 12:05:00, trying to wake up at 12:54:30 but waking up at ~12:48:45 -> correction = (54.5 - 48.75) / (48.75 - 5.00) -> correction = 0.17f
