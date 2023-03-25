#include "usb_midi_packet.h"

#define SYSEX_START_BYTE 0xF0
#define SYSEX_END_BYTE	 0xF7

static enum usb_midi_error_t channel_msg_cin(uint8_t first_byte, uint8_t *cin)
{
	uint8_t high_nibble = first_byte >> 4;

	switch (high_nibble) {
	case 0x8: /* Note off */
	case 0x9: /* Note on */
	case 0xa: /* Poly KeyPress */
	case 0xb: /* Control Change */
	case 0xe: /* PitchBend Change */
		/* Three byte channel Voice Message */
		*cin = high_nibble;
		break;
	case 0xc: /* Program Change */
	case 0xd: /* Channel Pressure */
		/* Two byte channel Voice Message */
		*cin = high_nibble;
		break;
	default:
		/* Invalid status byte */
		return USB_MIDI_ERROR_INVALID_MIDI_MSG;
	}

	/* Valid status byte */
	return USB_MIDI_SUCCESS;
}

static enum usb_midi_error_t non_sysex_system_msg_cin(uint8_t first_byte, uint8_t *cin)
{
	switch (first_byte) {
	case 0xf1: /* MIDI Time Code Quarter Frame */
	case 0xf3: /* Song Select */
		/* 2 byte System Common message */
		*cin = USB_MIDI_CIN_SYSCOM_2BYTE;
		break;
	case 0xf2: /* Song Position Pointer */
		/* 3 byte System Common message */
		*cin = USB_MIDI_CIN_SYSCOM_3BYTE;
		break;
	case 0xf6: /* Tune request */
		/* Single-byte System Common Message */
		*cin = USB_MIDI_CIN_SYS_COMMON_OR_SYSEX_END_1BYTE;
		break;
	case 0xf8: /* Timing Clock */
	case 0xfa: /* Start */
	case 0xfb: /* Continue */
	case 0xfc: /* Stop */
	case 0xfe: /* Active Sensing */
	case 0xff: /* System Reset */
		/* 1 byte system real time */
		*cin = USB_MIDI_CIN_1BYTE_DATA;
		break;
	default:
		/* Invalid status byte */
		return USB_MIDI_ERROR_INVALID_MIDI_MSG;
	}

	/* Valid status byte */
	return USB_MIDI_SUCCESS;
}

static enum usb_midi_error_t sysex_msg_cin(uint8_t *midi_bytes, uint8_t *cin)
{
	int is_data_byte[3] = {midi_bytes[0] < 0x80, midi_bytes[1] < 0x80, midi_bytes[2] < 0x80};

	if (midi_bytes[0] == SYSEX_START_BYTE) {
		if (midi_bytes[1] == SYSEX_END_BYTE) {
			/* Sysex case 1: F0 F7 */
			*cin = USB_MIDI_CIN_SYSEX_END_2BYTE;
		} else if (is_data_byte[1]) {
			if (midi_bytes[2] == SYSEX_END_BYTE) {
				/* Sysex case 2: F0 d F7 */
				*cin = USB_MIDI_CIN_SYSEX_END_3BYTE;
			} else if (is_data_byte[2]) {
				/* Sysex case 3: F0 d d */
				*cin = USB_MIDI_CIN_SYSEX_START_OR_CONTINUE;
			}
		}
	} else if (is_data_byte[0]) {
		if (is_data_byte[1]) {
			if (is_data_byte[2]) {
				/* Sysex case 4: d d d */
				*cin = USB_MIDI_CIN_SYSEX_START_OR_CONTINUE;
			} else if (midi_bytes[2] == SYSEX_END_BYTE) {
				/* Sysex case 5: d d F7 */
				*cin = USB_MIDI_CIN_SYSEX_END_3BYTE;
			}
		} else if (midi_bytes[1] == SYSEX_END_BYTE) {
			/* Sysex case 6: d F7 */
			*cin = USB_MIDI_CIN_SYSEX_END_2BYTE;
		}
	} else if (midi_bytes[0] == SYSEX_END_BYTE) {
		/* Sysex case 7: F7 */
		*cin = USB_MIDI_CIN_SYS_COMMON_OR_SYSEX_END_1BYTE;
	} else {
		/* Invalid sysex sequence */
		return USB_MIDI_ERROR_INVALID_MIDI_MSG;
	}

	/* Valid sysex sequence */
	return USB_MIDI_SUCCESS;
}

static uint8_t num_midi_bytes_for_cin(uint8_t cin)
{
	switch (cin) {
	case USB_MIDI_CIN_MISC:
	case USB_MIDI_CIN_CABLE_EVENT:
		/* Reserved for future expansion. Ignore. */
		return 0;
	case USB_MIDI_CIN_SYS_COMMON_OR_SYSEX_END_1BYTE:
	case USB_MIDI_CIN_1BYTE_DATA:
		return 1;
	case USB_MIDI_CIN_SYSCOM_2BYTE:
	case USB_MIDI_CIN_SYSEX_END_2BYTE:
	case USB_MIDI_CIN_PROGRAM_CHANGE:
	case USB_MIDI_CIN_CHANNEL_PRESSURE:
		return 2;
	default:
		return 3;
	}
}

enum usb_midi_error_t usb_midi_packet_from_midi_bytes(uint8_t *midi_bytes, uint8_t cable_num,
						      struct usb_midi_packet_t *packet)
{
	/* Building a USB MIDI packet from a MIDI message amounts to determining the code
	 * index number (CIN) corresponding to the message. This in turn determines the
	 * size of the MIDI message.
	 *
	 * The MIDI message is assumed to not contain interleaved system real time bytes.
	 *
	 * A MIDI message contained in a USB MIDI packet is 1, 2 or 3 bytes long. It either
	 *
	 * 1. is a channel message starting with one of the follwing status bytes
	 *    followed by data bytes: (n is the MIDI channel)
	 *
	 *    8n - note off, cin 0x8
	 *    9n - note on, cin 0x9
	 *    An - Polyphonic aftertouch, cin 0xa
	 *    Bn - Control change, cin 0xb
	 *    Cn - Program change, cin 0xc
	 *    Dn - Channel aftertouch, cin 0xd
	 *    En - Pitch bend change, cin 0xe
	 *
	 * 2. is a non-sysex system message starting with one of the following
	 *    status bytes followed by zero or more data bytes
	 *    (F4, F5, F9 and FD are undefined. F0, F7 are sysex)
	 *
	 *    F1 - MIDI Time Code Qtr. Frame, cin 0x2
	 *    F2 - Song Position Pointer, cin 0x3
	 *    F3 - Song Select, cin 0x2
	 *    F6 - Tune request, cin 0x5
	 *    F8 - Timing clock, cin 0xf
	 *    FA - Start, cin 0xf
	 *    FB - Continue, cin  0xf
	 *    FC - Stop, cin  0xf
	 *    FE - Active Sensing, cin  0xf
	 *    FF - System reset, cin 0xf
	 *
	 * 3. is a (partial) sysex message, taking one of the following forms (d is a data byte)
	 *    F0, F7      - sysex case 1, cin 0x6 (SysEx ends with following two bytes)
	 *    F0, d, F7   - sysex case 2, cin 0x7 (SysEx ends with following three bytes)
	 *    F0, d, d    - sysex case 3, cin 0x4 (SysEx starts or continues)
	 *    d, d, d     - sysex case 4, cin 0x4 (SysEx starts or continues)
	 *    d, d, F7    - sysex case 5, cin 0x7 (SysEx ends with following three bytes)
	 *    d, F7       - sysex case 6, cin 0x6 (SysEx ends with following two bytes)
	 *    F7          - sysex case 7, cin 0x5 (Single-byte System Common Message or
	 *                                         SysEx ends with following single byte.)
	 */

	if (cable_num >= 16) {
		return USB_MIDI_ERROR_INVALID_CABLE_NUM;
	}

	packet->cable_num = cable_num;
	packet->cin = 0;
	packet->num_midi_bytes = 0;

	enum usb_midi_error_t cin_error = channel_msg_cin(midi_bytes[0], &packet->cin);
	if (cin_error != USB_MIDI_SUCCESS) {
		cin_error = non_sysex_system_msg_cin(midi_bytes[0], &packet->cin);
	}
	if (cin_error != USB_MIDI_SUCCESS) {
		cin_error = sysex_msg_cin(midi_bytes, &packet->cin);
	}

	packet->num_midi_bytes = num_midi_bytes_for_cin(packet->cin);

	if (cin_error != USB_MIDI_SUCCESS || packet->num_midi_bytes == 0) {
		/* Invalid MIDI message. */
		return USB_MIDI_ERROR_INVALID_MIDI_MSG;
	}

	/* Put cable number and CIN in packet byte 0 */
	packet->bytes[0] = (packet->cable_num << 4) | packet->cin;

	/* Fill packet bytes 1,2 and 3 with zero padded midi bytes. */
	packet->bytes[1] = 0;
	packet->bytes[2] = 0;
	packet->bytes[3] = 0;
	for (int i = 0; i < packet->num_midi_bytes; i++) {
		packet->bytes[i + 1] = midi_bytes[i];
	}

	/* No errors */
	return USB_MIDI_SUCCESS;
}

enum usb_midi_error_t usb_midi_packet_from_usb_bytes(uint8_t *packet_bytes,
						     struct usb_midi_packet_t *packet)
{
	/* Parsing a USB MIDI packet amounts to deriving the number of MIDI
	 * bytes from the code index number (CIN) of the packet.
	 */

	/* packet header */
	packet->bytes[0] = packet_bytes[0];
	/* MIDI bytes */
	packet->bytes[1] = packet_bytes[1];
	packet->bytes[2] = packet_bytes[2];
	packet->bytes[3] = packet_bytes[3];

	packet->cable_num = packet_bytes[0] >> 4;
	packet->cin = packet_bytes[0] & 0xf;
	packet->num_midi_bytes = num_midi_bytes_for_cin(packet->cin);

	if (packet->num_midi_bytes == 0) {
		return USB_MIDI_ERROR_INVALID_CIN;
	}

	/* No errors */
	return USB_MIDI_SUCCESS;
}

enum usb_midi_error_t usb_midi_parse_packet(uint8_t *packet_bytes,
					    struct usb_midi_parse_cb_t *parse_cb)
{
	struct usb_midi_packet_t packet;
	enum usb_midi_error_t rc = usb_midi_packet_from_usb_bytes(packet_bytes, &packet);
	if (rc != USB_MIDI_SUCCESS) {
		return rc;
	}

	switch (packet.cin) {
	case USB_MIDI_CIN_SYSCOM_2BYTE:
	case USB_MIDI_CIN_SYSCOM_3BYTE:
	case USB_MIDI_CIN_NOTE_ON:
	case USB_MIDI_CIN_NOTE_OFF:
	case USB_MIDI_CIN_POLY_KEYPRESS:
	case USB_MIDI_CIN_CONTROL_CHANGE:
	case USB_MIDI_CIN_PROGRAM_CHANGE:
	case USB_MIDI_CIN_CHANNEL_PRESSURE:
	case USB_MIDI_CIN_PITCH_BEND_CHANGE:
	case USB_MIDI_CIN_1BYTE_DATA:
		if (parse_cb->message_cb) {
			parse_cb->message_cb(&packet.bytes[1], packet.num_midi_bytes,
					     packet.cable_num);
		}
		break;
	case USB_MIDI_CIN_SYSEX_START_OR_CONTINUE: {
		/*
		 * Possible cases:
		 * F0, d, d
		 * d, d, d
		 */
		int first_data_byte_idx = 1;
		int last_data_byte_idx = 3;
		if (packet.bytes[1] == SYSEX_START_BYTE) {
			if (parse_cb->sysex_start_cb) {
				parse_cb->sysex_start_cb(packet.cable_num);
			}
			first_data_byte_idx++;
		}

		int num_data_bytes = 1 + last_data_byte_idx - first_data_byte_idx;
		if (parse_cb->sysex_data_cb) {
			parse_cb->sysex_data_cb(&packet.bytes[first_data_byte_idx], num_data_bytes,
						packet.cable_num);
		}
		break;
	}

	case USB_MIDI_CIN_SYS_COMMON_OR_SYSEX_END_1BYTE:
		if (packet.bytes[1] != SYSEX_END_BYTE) {
			/* Single byte system common */
			if (parse_cb->message_cb) {
				parse_cb->message_cb(&packet.bytes[1], 1, packet.cable_num);
			}
		} else if (parse_cb->sysex_end_cb) {
			/* Sysex end */
			parse_cb->sysex_end_cb(packet.cable_num);
		}
		break;
	case USB_MIDI_CIN_SYSEX_END_2BYTE:
		/* Possible cases
			F0, F7
			d, F7
		*/
		if (packet.bytes[1] == SYSEX_START_BYTE) {
			if (parse_cb->sysex_start_cb) {
				parse_cb->sysex_start_cb(packet.cable_num);
			}
		} else if (parse_cb->sysex_data_cb) {
			parse_cb->sysex_data_cb(&packet.bytes[1], 1, packet.cable_num);
		}
		if (parse_cb->sysex_end_cb) {
			parse_cb->sysex_end_cb(packet.cable_num);
		}
		break;
	case USB_MIDI_CIN_SYSEX_END_3BYTE: {
		/* possible cases
		 *    d, d, F7
		 *    F0, d, F7
		 */
		int first_data_byte_idx = 1;
		int last_data_byte_idx = 2;
		if (packet.bytes[1] == SYSEX_START_BYTE) {
			if (parse_cb->sysex_start_cb) {
				parse_cb->sysex_start_cb(packet.cable_num);
			}
			first_data_byte_idx++;
		}
		int num_data_bytes = 1 + last_data_byte_idx - first_data_byte_idx;
		if (parse_cb->sysex_data_cb) {
			parse_cb->sysex_data_cb(&packet.bytes[first_data_byte_idx], num_data_bytes,
						packet.cable_num);
		}
		if (parse_cb->sysex_end_cb) {
			parse_cb->sysex_end_cb(packet.cable_num);
		}
		break;
	}

	default:
		return USB_MIDI_ERROR_INVALID_CIN;
	}

	return USB_MIDI_SUCCESS;
}