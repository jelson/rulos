#include "gpsinput.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

uint8_t split(char *s, uint8_t *index /*OUT*/, uint8_t index_size);
void _gpsinput_process_sentence(UartState_t *uart, void *user_data,
                                char *sentence);
bool _parse_ddm(char *s, float *out);
char *_index(char *s, char c);
float _atofi(char *s);

void gpsinput_init(GPSInput *gpsi, uint8_t uart_id,
                   ActivationFuncPtr data_ready_cb_func,
                   void *data_ready_cb_data) {
  uart_init(&gpsi->uart, uart_id, 4800, TRUE);
  linereader_init(&gpsi->linereader, &gpsi->uart, _gpsinput_process_sentence,
                  gpsi);

  gpsi->lat = 0.;
  gpsi->lon = 0.;
  gpsi->data_ready_cb_func = data_ready_cb_func;
  gpsi->data_ready_cb_data = data_ready_cb_data;
}

uint8_t split(char *s, uint8_t *index /*OUT*/, uint8_t index_size) {
  uint8_t i, ii = 0;
  for (i = 0; s[i] != '\0'; i++) {
    if (s[i] == ',') {
      index[ii] = i + 1;
      ii++;
    }
    if (ii >= index_size) {
      return ii;
    }
  }
  return ii;
}

void _gpsinput_process_sentence(UartState_t *uart, void *user_data,
                                char *sentence) {
  GPSInput *gpsi = (GPSInput *)user_data;
#ifndef SIM
#define INVALID(m) \
  { return; }
#else
  // fprintf(stderr, "gpsinput sees %s\n", sentence);
#define INVALID(m)                                    \
  {                                                   \
    /*fprintf(stderr, "rejecting because %s\n", m);*/ \
    return;                                           \
  }
#endif

  if (memcmp(sentence, "$GPGGA,", 7) != 0) {
    INVALID("unknown sentence");
  }

  uint8_t field_index[15];
  uint8_t num_fields = split(sentence, field_index,
                             sizeof(field_index) / sizeof(field_index[0]));
  // $GPGGA,235453,4736.760,N,12219.740,W,1,05,2.2,104.1,M,-18.4,M,,*7A
  if (num_fields != 14) {
    INVALID("num_fields");
  }
#define FI_TIME     0
#define FI_LAT      1
#define FI_LAT_SIGN 2
#define FI_LON      3
#define FI_LON_SIGN 4
#define FI_QUALITY  5

  if (memcmp(&sentence[field_index[FI_LAT_SIGN]], "N,", 2) != 0) {
    INVALID("N");
  }
  if (memcmp(&sentence[field_index[FI_LON_SIGN]], "W,", 2) != 0) {
    INVALID("W");
  }
  if (memcmp(&sentence[field_index[FI_QUALITY]], "1,", 2) != 0) {
    INVALID("Quality");
  }
  bool rc;
  rc = _parse_ddm(&sentence[field_index[FI_LAT]], &gpsi->lat);
  if (!rc) {
    INVALID("lat ddm");
  }
  rc = _parse_ddm(&sentence[field_index[FI_LON]], &gpsi->lon);
  if (!rc) {
    INVALID("lon ddm");
  }
  gpsi->lon = -gpsi->lon;  // above, asserting West of prime meridian
  // notify client that a new fix is available
  gpsi->data_ready_cb_func(gpsi->data_ready_cb_data);
}

bool _parse_ddm(char *s, float *out) {
  char *decimal = _index(s, '.');
  if (decimal == NULL) {
    return false;
  }

  float min = _atofi(decimal - 2);
  float min_frac = _atofi(decimal + 1);
#ifdef SIM
//	fprintf(stderr, "min %f from %s\n", (double) min, decimal-2);
//	fprintf(stderr, "min_frac %f from %s\n", (double) min_frac, decimal+1);
#endif                    // SIM
  *(decimal - 2) = '\0';  // note stomp of string in place
  float deg = _atofi(s);
#ifdef SIM
//	fprintf(stderr, "deg %f from %s\n", (double) deg, s);
#endif  // SIM
  char *comma = _index(decimal, ',');
  if (comma == NULL) {
    return false;
  }
  uint8_t decimal_digits = comma - decimal - 1;
  while (decimal_digits > 0) {
    min_frac /= 10.0;
    decimal_digits--;
  }
  min += min_frac;
  *out = deg + min / 60.0;

  return true;
}

char *_index(char *s, char c) {
  for (; *s != '\0'; s++) {
    if (*s == c) {
      return s;
    }
  }
  return NULL;
}

float _atofi(char *s) {
  uint16_t v = 0;
  for (; *s >= '0' && *s <= '9'; s++) {
    v *= 10;
    v += (*s - '0');
  }
  return (float)v;
}
