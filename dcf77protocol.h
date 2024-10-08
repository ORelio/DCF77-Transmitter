/* DCF77 Protocol Encoding library */
/* (c) 2018-2020 Luigi Calligaris. GPL v2+ */
/* (c) 2023 ORelio - bug fixes and improvements */

#ifndef DCF77_PROTOCOL_H
#define DCF77_PROTOCOL_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DCF77_DATALEN 59
uint8_t dcf77_even_parity(uint8_t const* begin, uint8_t const* end);
char *dcf77_encode_data(struct tm* local_time, uint8_t* dcf77_one_minute_data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //DCF77_PROTOCOL_H
