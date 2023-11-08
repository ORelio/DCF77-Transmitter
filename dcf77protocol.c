/* DCF77 Protocol Encoding library */
/* (c) 2018-2020 Luigi Calligaris. GPL v2+ */
/* (c) 2023 ORelio - bug fixes and improvements */

#include <stdint.h>
#include <time.h>

// Calculates the even parity of an uint8_t array formatted following DCF77 format defined in dcf77_encode_data
// begin: first element of array
// end  : first-past-the-last element of the array
uint8_t dcf77_even_parity(uint8_t const* begin, uint8_t const* end)
{
	int parity = 1;
	
	for (uint8_t const* p = begin; p != end; ++p)
		if ((*p) == 1) // If the bit is set, swap the parity
			parity *= -1;
	
	return (parity == -1) ? 1 : 0;
}

// Encode local time into the format used by the DCF77 protocol
// https://en.wikipedia.org/wiki/DCF77#Time_code_interpretation
// https://www.ptb.de/cms/en/ptb/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77/dcf77-time-code.html
// https://github.com/SensorsIot/DCF77-Transmitter-for-ESP32/blob/master/DFC77_ESP32/DFC77_ESP32.ino
// There is a 1:1 encoding between the bit state expressed as an unsigned and the length of the OFF encoding, in units of 100ms
//  0:  100ms OFF keying, 900ms ON keying, meaning a 0 (reset bit)
//  1:  200ms OFF keying, 800ms ON keying, meaning a 1 (set bit)
void dcf77_encode_data(struct tm* local_time, uint8_t* dcf77_one_minute_data)
{
	dcf77_one_minute_data[ 0] = 0; // Start of minute. Always reset

	dcf77_one_minute_data[ 1] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented
	dcf77_one_minute_data[ 2] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented
	dcf77_one_minute_data[ 3] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[ 4] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented
	dcf77_one_minute_data[ 5] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[ 6] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[ 7] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[ 8] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[ 9] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[10] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[11] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[12] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[13] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented 
	dcf77_one_minute_data[14] = 0; // Third-party information content provided by BBK and Meteo Time. Not implemented

	dcf77_one_minute_data[15] = 0; // Alarm bit. 0 = normal operation, 1 = fault in transmitter operation. Not implemented

	dcf77_one_minute_data[16] = 0; // Summer time announcement. Set during hour before change. Not implemented
	dcf77_one_minute_data[17] = local_time->tm_isdst > 0 ? 1 : 0;  // Set when DST is in effect (tm_isdst: 0=no >0=yes <0=unknown)
	dcf77_one_minute_data[18] = local_time->tm_isdst > 0 ? 0 : 1;  // Set when DST is not in effect (tm_isdst: 0=no >0=yes <0=unknown)
	dcf77_one_minute_data[19] = 0; // Leap second announcement. Set during hour before leap second. Not implemented

	dcf77_one_minute_data[20] = 1; // Start of encoded time. Always set

	// Each code emitted contains the information for the following minute ("at minute mark, the time will be...")
	// i.e. the receiver loads transmitted information then starts counting at end of transmission (minute mark)
	uint8_t const minute = (local_time->tm_min + 1) % 60; // Transmit the next minute ahead of time
	uint8_t const minute_mod_10 = minute % 10;
	uint8_t const minute_div_10 = minute / 10;

	dcf77_one_minute_data[21] = ( (minute_mod_10 & 0b00000001) != 0 ); // First cypher of minute. bit 0
	dcf77_one_minute_data[22] = ( (minute_mod_10 & 0b00000010) != 0 ); // First cypher of minute. bit 1
	dcf77_one_minute_data[23] = ( (minute_mod_10 & 0b00000100) != 0 ); // First cypher of minute. bit 2
	dcf77_one_minute_data[24] = ( (minute_mod_10 & 0b00001000) != 0 ); // First cypher of minute. bit 3
	dcf77_one_minute_data[25] = ( (minute_div_10 & 0b00000001) != 0 ); // Second cypher of minute. bit 0
	dcf77_one_minute_data[26] = ( (minute_div_10 & 0b00000010) != 0 ); // Second cypher of minute. bit 1
	dcf77_one_minute_data[27] = ( (minute_div_10 & 0b00000100) != 0 ); // Second cypher of minute. bit 2
	dcf77_one_minute_data[28] = dcf77_even_parity(dcf77_one_minute_data + 21, dcf77_one_minute_data + 28); // minute parity

	uint8_t const hour = (local_time->tm_hour + (minute == 0 ? 1 : 0)) % 24; // If next minute is zero then we advanced to next hour
	uint8_t const hour_mod_10 = hour % 10;
	uint8_t const hour_div_10 = hour / 10;

	dcf77_one_minute_data[29] = ( (hour_mod_10 & 0b00000001) != 0 ); // First decimal cypher of hour. bit 0
	dcf77_one_minute_data[30] = ( (hour_mod_10 & 0b00000010) != 0 ); // First decimal cypher of hour. bit 1
	dcf77_one_minute_data[31] = ( (hour_mod_10 & 0b00000100) != 0 ); // First decimal cypher of hour. bit 2
	dcf77_one_minute_data[32] = ( (hour_mod_10 & 0b00001000) != 0 ); // First decimal cypher of hour. bit 3
	dcf77_one_minute_data[33] = ( (hour_div_10 & 0b00000001) != 0 ); // Second decimal cypher of hour. bit 0
	dcf77_one_minute_data[34] = ( (hour_div_10 & 0b00000010) != 0 ); // Second decimal cypher of hour. bit 1
	dcf77_one_minute_data[35] = dcf77_even_parity(dcf77_one_minute_data + 29, dcf77_one_minute_data + 35);  // hour parity

	uint8_t const day_month = local_time->tm_mday; // Here we should also advance to next day, month, etc when needed. Not implemented.
	uint8_t const day_month_mod_10 = day_month % 10;
	uint8_t const day_month_div_10 = day_month / 10;

	dcf77_one_minute_data[36] = ( (day_month_mod_10 & 0b00000001) != 0 ); // First decimal cypher of day of the month. bit 0
	dcf77_one_minute_data[37] = ( (day_month_mod_10 & 0b00000010) != 0 ); // First decimal cypher of day of the month. bit 1
	dcf77_one_minute_data[38] = ( (day_month_mod_10 & 0b00000100) != 0 ); // First decimal cypher of day of the month. bit 2
	dcf77_one_minute_data[39] = ( (day_month_mod_10 & 0b00001000) != 0 ); // First decimal cypher of day of the month. bit 3
	dcf77_one_minute_data[40] = ( (day_month_div_10 & 0b00000001) != 0 ); // Second decimal cypher of day of the month. bit 0
	dcf77_one_minute_data[41] = ( (day_month_div_10 & 0b00000010) != 0 ); // Second decimal cypher of day of the month. bit 1

	uint8_t day_week = local_time->tm_wday; // This variable counts from 0 (Sunday) to 6 (Saturday)
	if (day_week == 0) // DCF77 counts from 1 (Monday) to 7 (Sunday): Change Sunday's value from 0 to 7
		day_week = 7;

	dcf77_one_minute_data[42] = ( (day_week & 0b00000001) != 0 ); // Day of the week. Monday = 1, Sunday = 7
	dcf77_one_minute_data[43] = ( (day_week & 0b00000010) != 0 ); // Day of the week. Monday = 1, Sunday = 7
	dcf77_one_minute_data[44] = ( (day_week & 0b00000100) != 0 ); // Day of the week. Monday = 1, Sunday = 7

	uint8_t const month = 1 + local_time->tm_mon;
	uint8_t const month_mod_10 = month % 10;
	uint8_t const month_div_10 = month / 10;

	dcf77_one_minute_data[45] = ( (month_mod_10 & 0b00000001) != 0 ); // First decimal cypher of month number. bit 0
	dcf77_one_minute_data[46] = ( (month_mod_10 & 0b00000010) != 0 ); // First decimal cypher of month number. bit 1
	dcf77_one_minute_data[47] = ( (month_mod_10 & 0b00000100) != 0 ); // First decimal cypher of month number. bit 2
	dcf77_one_minute_data[48] = ( (month_mod_10 & 0b00001000) != 0 ); // First decimal cypher of month number. bit 3
	dcf77_one_minute_data[49] = ( (month_div_10 & 0b00000001) != 0 ); // Second decimal cypher of month number. bit 0

	uint8_t const year_mod_100 = local_time->tm_year % 100;
	uint8_t const year_mod_10 = year_mod_100 % 10;
	uint8_t const year_div_10 = year_mod_100 / 10;

	dcf77_one_minute_data[50] = ( (year_mod_10 & 0b00000001) != 0 ); // First cypher of year. bit 0
	dcf77_one_minute_data[51] = ( (year_mod_10 & 0b00000010) != 0 ); // First cypher of year. bit 1
	dcf77_one_minute_data[52] = ( (year_mod_10 & 0b00000100) != 0 ); // First cypher of year. bit 2
	dcf77_one_minute_data[53] = ( (year_mod_10 & 0b00001000) != 0 ); // First cypher of year. bit 3
	dcf77_one_minute_data[54] = ( (year_div_10 & 0b00000001) != 0 ); // Second cypher of year. bit 0
	dcf77_one_minute_data[55] = ( (year_div_10 & 0b00000010) != 0 ); // Second cypher of year. bit 1
	dcf77_one_minute_data[56] = ( (year_div_10 & 0b00000100) != 0 ); // Second cypher of year. bit 2
	dcf77_one_minute_data[57] = ( (year_div_10 & 0b00001000) != 0 ); // Second cypher of year. bit 3
	dcf77_one_minute_data[58] = dcf77_even_parity(dcf77_one_minute_data + 36, dcf77_one_minute_data + 58); // date parity

	//dcf77_one_minute_data[59] = Minute mark / End of transmission. This second is without modulation (no data transmitted)
}
