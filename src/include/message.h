#ifndef _message_h
#define _message_h

typedef uint8_t Port;

// Wire format for packets.  Format:
// Dest port
// Checksum
// Payload_Len
// [Payload_Len * uint8_t]
typedef struct s_message {
	Port dest_port;
	uint8_t checksum;
	uint8_t payload_len;
	char data[0];
} Message, *MessagePtr;

#endif // _message_h
