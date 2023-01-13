#include "usb_midi_packet.h"

bool channel_msg_cin(uint8_t first_byte, uint8_t* cin)
{
	uint8_t high_nibble = first_byte >> 4;

	switch (high_nibble)
	{
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
		return false;
	}

        /* Valid status byte */
	return true;
}

bool non_sysex_system_msg_cin(uint8_t first_byte, uint8_t* cin)
{
	switch (first_byte)
	{
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
		*cin = USB_MIDI_CIN_SYSEX_END_1BYTE;
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
		return false;
	}

        /* Valid status byte */
	return true;
}

bool sysex_msg_cin(uint8_t* midi_bytes, uint8_t* cin)
{
	bool is_data_byte[3] = {
			midi_bytes[0] < 0x80,
			midi_bytes[1] < 0x80,
			midi_bytes[2] < 0x80};

	if (midi_bytes[0] == 0xf0 && midi_bytes[1] == 0xf7)
	{
		/* Sysex case 1: F0 F7 */
		*cin = USB_MIDI_CIN_SYSEX_END_2BYTE;
	}
	else if (midi_bytes[0] == 0xf0 && is_data_byte[1] && midi_bytes[2] == 0xf7)
	{
		/* Sysex case 2: F0 d F7 */
		*cin = USB_MIDI_CIN_SYSEX_END_3BYTE;
	}
	else if (midi_bytes[0] == 0xf0 && is_data_byte[1] && is_data_byte[2])
	{
		/* Sysex case 3: F0 d d */
		*cin = USB_MIDI_CIN_SYSEX_START_OR_CONTINUE;
	}
	else if (is_data_byte[0] && is_data_byte[1] && is_data_byte[2])
	{
		/* Sysex case 4: d d d */
		*cin = USB_MIDI_CIN_SYSEX_START_OR_CONTINUE;
	}
	else if (is_data_byte[0] && is_data_byte[1] && midi_bytes[2] == 0xf7)
	{
		/* Sysex case 5: d d F7 */
		*cin = USB_MIDI_CIN_SYSEX_END_3BYTE;
	}
	else if (is_data_byte[0] && midi_bytes[1] == 0xf7)
	{
		/* Sysex case 6: d F7 */
		*cin = USB_MIDI_CIN_SYSEX_END_2BYTE;
	}
	else if (midi_bytes[0] == 0xf7)
	{
		/* Sysex case 7: F7 */
		*cin = USB_MIDI_CIN_SYSEX_END_1BYTE;
	}
	else
	{
		/* Invalid sysex sequence */
		return false;
	}

        /* Valid sysex sequence */
	return true;
}

uint8_t num_midi_bytes_for_cin(uint8_t cin)
{
	switch (cin)
	{
	case USB_MIDI_CIN_MISC:
	case USB_MIDI_CIN_CABLE_EVENT:
		/* Reserved for future expansion. Ignore. */
		return 0;
	case USB_MIDI_CIN_SYSEX_END_1BYTE:
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

uint32_t usb_midi_packet_from_midi_bytes(uint8_t *midi_bytes, uint8_t cable_num, struct usb_midi_packet *packet)
{
	/* Building a USB MIDI packet from a MIDI message amounts to determining the code
	 * index number (CIN) corresponding to the message. This in turn determines the
	 * size of the MIDI message.
	 *
	 * The MIDI message is assumed to not contain interleaved system real time bytes.
	 *
	 * A MIDI message contained in a USB MIDI packet is 1, 2 or 3 bytes long. It either
	 *
	 * 1. is a channel message starting with one of the follwing status bytes: (n is the MIDI channel)
	 *
	 *    8n - note off, cin 0x8
	 *    9n - note on, cin 0x9
	 *    An - Polyphonic aftertouch, cin 0xa
	 *    Bn - Control change, cin 0xb
	 *    Cn - Program change, cin 0xc
	 *    Dn - Channel aftertouch, cin 0xd
	 *    En - Pitch bend change, cin 0xe
	 *
	 * 2. is a non-sysex system message starting with one of the following status bytes
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
	 * 3. is a (partial) sysex sequence, taking one of the following forms (d is a data byte)
	 *    F0, F7      - sysex case 1, cin 0x6 (SysEx ends with following two bytes)
	 *    F0, d, F7   - sysex case 2, cin 0x7 (SysEx ends with following three bytes)
	 *    F0, d, d    - sysex case 3, cin 0x4 (SysEx starts or continues)
	 *    d, d, d     - sysex case 4, cin 0x4 (SysEx starts or continues)
	 *    d, d, F7    - sysex case 5, cin 0x7 (SysEx ends with following three bytes)
	 *    d, F7       - sysex case 6, cin 0x6 (SysEx ends with following two bytes)
	 *    F7          - sysex case 7, cin 0x5 (Single-byte System Common Message or
	 *                                         SysEx ends with following single byte.)
	 */

	if (cable_num >= 16)
	{
		return EINVAL;
	}

	packet->cable_num = cable_num;
	packet->cin = 0;
	packet->num_midi_bytes = 0;

	bool did_set_cin = channel_msg_cin(midi_bytes[0], &packet->cin);
	if (!did_set_cin)
	{
		did_set_cin = non_sysex_system_msg_cin(midi_bytes[0], &packet->cin);
	}
	if (!did_set_cin)
	{
		did_set_cin = sysex_msg_cin(midi_bytes, &packet->cin);
	}

	packet->num_midi_bytes = num_midi_bytes_for_cin(packet->cin);

        if (!did_set_cin || packet->num_midi_bytes == 0)
	{
		/* Invalid MIDI message. */
		return EINVAL;
	}

	/* Put cable number and CIN in packet byte 0 */
	packet->bytes[0] = (packet->cable_num << 4) | packet->cin;

	/* Fill packet bytes 1,2 and 3 with zero padded midi bytes. */
        packet->bytes[1] = 0;
        packet->bytes[2] = 0;
        packet->bytes[3] = 0;
	for (int i = 0; i < packet->num_midi_bytes; i++)
	{
		packet->bytes[i + 1] = midi_bytes[i];
	}

        /* No errors */
	return 0;
}

uint32_t usb_midi_packet_from_usb_bytes(uint8_t *packet_bytes, struct usb_midi_packet *packet)
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

	if (packet->num_midi_bytes == 0)
	{
		return EINVAL;
	}

        /* No errors */
	return 0;

	// LOG_DBG("Parsed packet cin %d, num_midi_bytes %d", packet->cin, packet->num_midi_bytes);
}