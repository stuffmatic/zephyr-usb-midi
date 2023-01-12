#ifndef ZEPHYR_USB_MIDI_PACKET_H_
#define ZEPHYR_USB_MIDI_PACKET_H_

#include <zephyr/zephyr.h>

/* Code Index Numbers. See table 4-1 in the spec. */
enum usb_midi_cin
{
        /* Miscellaneous function codes. Reserved for future extensions. */
        USB_MIDI_CIN_MISC = 0x0,
        /* Cable events. Reserved for future expansion. */
        USB_MIDI_CIN_CABLE_EVENT = 0x1,
        /* Two-byte System Common messages like MTC, SongSelect, etc. */
        USB_MIDI_CIN_SYSCOM_2BYTE = 0x2,
        /* Three-byte System Common messages like SPP, etc. */
        USB_MIDI_CIN_SYSCOM_3BYTE = 0x3,
        /* SysEx starts or continues */
        USB_MIDI_CIN_SYSEX_START_OR_CONTINUE = 0x4,
        /* Single-byte System Common Message or SysEx ends with following single byte. */
        USB_MIDI_CIN_SYSEX_END_1BYTE = 0x5,
        /* SysEx ends with following two bytes. */
        USB_MIDI_CIN_SYSEX_END_2BYTE = 0x6,
        /* SysEx ends with following three bytes. */
        USB_MIDI_CIN_SYSEX_END_3BYTE = 0x7,
        /* Note-off */
        USB_MIDI_CIN_NOTE_ON = 0x8,
        /* Note-on */
        USB_MIDI_CIN_NOTE_OFF = 0x9,
        /* Poly-KeyPress */
        USB_MIDI_CIN_POLY_KEYPRESS = 0xA,
        /* Control Change */
        USB_MIDI_CIN_CONTROL_CHANGE = 0xB,
        /* Program Change */
        USB_MIDI_CIN_PROGRAM_CHANGE = 0xC,
        /* Channel Pressure */
        USB_MIDI_CIN_CHANNEL_PRESSURE = 0xD,
        /* PitchBend Change */
        USB_MIDI_CIN_PITCH_BEND_CHANGE = 0xE,
        /* Single Byte */
        USB_MIDI_CIN_1BYTE_DATA = 0xF
};

/* A USB MIDI event packet. See chapter 4 in the spec. */
struct usb_midi_packet
{
        uint8_t cable_num;
        uint8_t cin;
        uint8_t bytes[4];
        uint8_t num_midi_bytes;
};

uint32_t usb_midi_packet_from_midi_bytes(uint8_t* midi_bytes, uint8_t cable_num, struct usb_midi_packet *packet);

uint32_t usb_midi_packet_from_usb_bytes(uint8_t* packet_bytes, struct usb_midi_packet *packet);

#endif