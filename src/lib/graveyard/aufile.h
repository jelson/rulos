#ifndef _AUFILE_H
#define _AUFILE_H

// http://www.opengroup.org/public/pubs/external/auformat.html

typedef struct {
	uint32_t magic;
	uint32_t hdr_size;
	uint32_t data_size;
	uint32_t encoding;
	uint32_t sample_rate;
	uint32_t channels;
} AuHeader;

#define AUDIO_FILE_MAGIC ((uint32_t)0x2e736e64)

#define AUDIO_UNKNOWN_SIZE (~0) /* (unsigned) -1 */

#define AUDIO_FILE_ENCODING_MULAW_8 (1) /* 8-bit ISDN u-law */
#define AUDIO_FILE_ENCODING_LINEAR_8 (2) /* 8-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_16 (3) /* 16-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_24 (4) /* 24-bit linear PCM */ 
#define AUDIO_FILE_ENCODING_LINEAR_32 (5) /* 32-bit linear PCM */ 
#define AUDIO_FILE_ENCODING_FLOAT (6) /* 32-bit IEEE floating point */ 
#define AUDIO_FILE_ENCODING_DOUBLE (7) /* 64-bit IEEE floating point */ 
#define AUDIO_FILE_ENCODING_ADPCM_G721 (23) /* 4-bit CCITT g.721 ADPCM */ 
#define AUDIO_FILE_ENCODING_ADPCM_G722 (24) /* CCITT g.722 ADPCM */ 
#define AUDIO_FILE_ENCODING_ADPCM_G723_3 (25) /* CCITT g.723 3-bit ADPCM */ 
#define AUDIO_FILE_ENCODING_ADPCM_G723_5 (26) /* CCITT g.723 5-bit ADPCM */ 
#define AUDIO_FILE_ENCODING_ALAW_8 (27) /* 8-bit ISDN A-law */

#endif // _AUFILE_H
