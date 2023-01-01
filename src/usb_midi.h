#ifndef ZEPHYR_USB_MIDI_H_
#define ZEPHYR_USB_MIDI_H_

#include <zephyr/init.h>

struct usb_midi_ac_if_descriptor
{
    uint8_t bLength;            // 0x09 Size of this descriptor, in bytes.
    uint8_t bDescriptorType;    // 1 0x24 CS_INTERFACE.
    uint8_t bDescriptorSubtype; // 1 0x01 HEADER subtype.
    uint16_t bcdADC;            // 2 0x0100 Revision of class specification - 1.0
    uint16_t wTotalLength;      // 2 0x0009 Total size of class specific descriptors.
    uint8_t bInCollection;      // 1 0x01 Number of streaming interfaces.
    uint8_t baInterfaceNr;      // 1 0x01 MIDIStreaming interface 1 belongs to this AudioControl interface.
} __packed;

struct usb_midi_ms_if_descriptor {
	uint8_t bLength; // 1 0x07 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; // 1 0x24 CS_INTERFACE descriptor.
    uint8_t bDescriptorSubtype; // 1 0x01 MS_HEADER subtype.
    uint16_t BcdADC; // 2 0x0100 Revision of this class specification.
    uint16_t wTotalLength; // 2 0x0041 Total size of class-specific descriptors .
} __packed;

struct usb_midi_in_jack_descriptor {
    uint8_t bLength; // 1 0x06 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; // 1 0x24 CS_INTERFACE descriptor.
    uint8_t bDescriptorSubtype; // 1 0x02 MIDI_IN_JACK subtype.
    uint8_t bJackType; // 1 0x01 EMBEDDED.
    uint8_t bJackID; // 1 0x01 ID of this Jack.
    uint8_t iJack; // 1 0x00 Unused
} __packed;

struct usb_midi_out_jack_descriptor {
    uint8_t bLength; // 1 0x09 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; // 1 0x24 CS_INTERFACE descriptor.
    uint8_t bDescriptorSubtype; // 1 0x03 MIDI_OUT_JACK subtype.
    uint8_t bJackType; // 1 0x01 EMBEDDED.
    uint8_t bJackID; // 1 0x03 ID of this Jack.
    uint8_t bNrInputPins; // 1 0x01 Number of Input Pins of this Jack.
    uint8_t BaSourceID; //(1) 1 0x02 ID of the Entity to which this Pin is connected.
    uint8_t BaSourcePin; //(1) 1 0x01 Output Pin number of the Entity to which this Input Pin is connected.
    uint8_t iJack; // 1 0x00 Unused.
} __packed;

struct usb_midi_bulk_out_ep_descriptor {
    uint8_t bLength; // 1 0x05 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; // 1 0x25 CS_ENDPOINT descriptor
    uint8_t bDescriptorSubtype; // 1 0x01 MS_GENERAL subtype.
    uint8_t bNumEmbMIDIJack; // 1 0x01 Number of embedded MIDI IN Jacks.
    uint8_t BaAssocJackID; // (1) 1 0x01 ID of the Embedded MIDI IN Jack.
} __packed;

struct usb_midi_bulk_in_ep_descriptor {
    uint8_t bLength; //  1 0x05 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; //  1 0x25 CS_ENDPOINT descriptor
    uint8_t bDescriptorSubtype; //  1 0x01 MS_GENERAL subtype.
    uint8_t bNumEmbMIDIJack; //  1 0x01 Number of embedded MIDI OUT Jacks.
    uint8_t BaAssocJackID; // (1) 1 0x03 ID of the Embedded MIDI OUT Jack.
} __packed;

#endif