#ifndef ZEPHYR_USB_MIDI_INTERNAL_H_
#define ZEPHYR_USB_MIDI_INTERNAL_H_

#include <zephyr/init.h>

// A USB MIDI data packet.
struct usb_midi_packet
{
        uint8_t cin;
        uint8_t cable_num;
        uint8_t bytes[4];
        uint8_t num_midi_bytes;
};

// The number of MIDI input ports
#define USB_MIDI_NUM_INPUTS CONFIG_USB_MIDI_NUM_INPUTS
// The number of MIDI output ports
#define USB_MIDI_NUM_OUTPUTS CONFIG_USB_MIDI_NUM_OUTPUTS

// Code Index Numbers. See table 4-1 in the spec.
enum usb_midi_cin
{
        USB_MIDI_CIN_MISC = 0x0,
        USB_MIDI_CIN_CABLE_EVENT = 0x1,
        USB_MIDI_CIN_SYSCOM_2BYTE = 0x2,
        USB_MIDI_CIN_SYSCOM_3BYTE = 0x3,
        USB_MIDI_CIN_SYSEX_START = 0x4,
        USB_MIDI_CIN_SYSEX_END_1BYTE = 0x5,
        USB_MIDI_CIN_SYSEX_END_2BYTE = 0x6,
        USB_MIDI_CIN_SYSEX_END_3BYTE = 0x7,
        USB_MIDI_CIN_NOTE_ON = 0x8,
        USB_MIDI_CIN_NOTE_OFF = 0x9,
        USB_MIDI_CIN_POLY_KEYPRESS = 0xA,
        USB_MIDI_CIN_CONTROL_CHANGE = 0xB,
        USB_MIDI_CIN_PROGRAM_CHANGE = 0xC,
        USB_MIDI_CIN_CHANNEL_PRESSURE = 0xD,
        USB_MIDI_CIN_PITCH_BEND_CHANGE = 0xE,
        USB_MIDI_CIN_1BYTE_DATA = 0xF
};

// MS (MIDI streaming) Class-Specific Interface Descriptor Subtypes. See table A.1 in the spec.
enum usb_midi_if_desc_subtype
{
        USB_MIDI_IF_DESC_UNDEFINED = 0x00,
        USB_MIDI_IF_DESC_MS_HEADER = 0x01,
        USB_MIDI_IF_DESC_MIDI_IN_JACK = 0x02,
        USB_MIDI_IF_DESC_MIDI_OUT_JACK = 0x03,
        USB_MIDI_IF_DESC_ELEMENT = 0x04
};

// MS Class-Specific Endpoint Descriptor Subtypes. See table A.2 in the spec.
enum usb_midi_ep_desc_subtype
{
        USB_MIDI_EP_DESC_UNDEFINED = 0x00,
        USB_MIDI_EP_DESC_MS_GENERAL = 0x01
};

// MS MIDI IN and OUT Jack types. See table A.3 in the spec.
enum usb_midi_jack_type
{
        USB_MIDI_JACK_TYPE_UNDEFINED = 0x00,
        USB_MIDI_JACK_TYPE_EMBEDDED = 0x01,
        USB_MIDI_JACK_TYPE_EXTERNAL = 0x02
};

// Class-specific AC (audio control) Interface Descriptor.
struct usb_midi_ac_if_descriptor
{
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint16_t bcdADC;
        uint16_t wTotalLength;
        uint8_t bInCollection;
        uint8_t baInterfaceNr;
} __packed;

// Class-Specific MS Interface Header Descriptor. See table 6.2 in the spec.
struct usb_midi_ms_if_descriptor
{
        // Size of this descriptor, in bytes
        uint8_t bLength;
        // CS_INTERFACE descriptor type
        uint8_t bDescriptorType;
        // MS_HEADER descriptor subtype.
        uint8_t bDescriptorSubtype;
        // MIDIStreaming SubClass Specification Release Number in Binary-Coded Decimal.
        // Currently 01.00.
        uint16_t BcdADC;
        // Total number of bytes returned for the class-specific MIDIStreaming interface descriptor.
        // Includes the combined length of this descriptor header and all Jack and Element descriptors.
        uint16_t wTotalLength;
} __packed;

// MIDI IN Jack Descriptor. See table 6.3 in the spec.
struct usb_midi_in_jack_descriptor
{
        // Size of this descriptor, in bytes.
        uint8_t bLength;
        // CS_INTERFACE descriptor type.
        uint8_t bDescriptorType;
        // MIDI_IN_JACK descriptor subtype.
        uint8_t bDescriptorSubtype;
        // EMBEDDED or EXTERNAL
        uint8_t bJackType;
        // Constant uniquely identifying the MIDI IN Jack within the USB-MIDI function.
        uint8_t bJackID;
        // Index of a string descriptor, describing the MIDI IN Jack.
        uint8_t iJack;
} __packed;

// MIDI OUT Jack Descriptor. See table 6.4 in the spec.
struct usb_midi_out_jack_descriptor
{
        // Size of this descriptor, in bytes:
        uint8_t bLength;
        // CS_INTERFACE descriptor type.
        uint8_t bDescriptorType;
        // MIDI_OUT_JACK descriptor subtype.
        uint8_t bDescriptorSubtype;
        // EMBEDDED or EXTERNAL
        uint8_t bJackType;
        // Constant uniquely identifying the MIDI OUT Jack within the USB-MIDI function.
        uint8_t bJackID;
        // Number of Input Pins of this MIDI OUT Jack (assumed to be 1 in this implementation).
        uint8_t bNrInputPins;
        // ID of the Entity to which the first Input Pin of this MIDI OUT Jack is connected.
        uint8_t BaSourceID;
        // Output Pin number of the Entity to which the first Input Pin of this MIDI OUT Jack is connected.
        uint8_t BaSourcePin;
        // Index of a string descriptor, describing the MIDI OUT Jack.
        uint8_t iJack;
} __packed;

// Class-Specific MS Bulk Data Endpoint Descriptor corresponding to a MIDI output.
// See table 6-7 in the spec.
struct usb_midi_bulk_out_ep_descriptor
{
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t bNumEmbMIDIJack;
        uint8_t BaAssocJackID[USB_MIDI_NUM_INPUTS];
} __packed;

// Class-Specific MS Bulk Data Endpoint Descriptor corresponding to a MIDI input.
// See table 6-7 in the spec.
struct usb_midi_bulk_in_ep_descriptor
{
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t bNumEmbMIDIJack;
        uint8_t BaAssocJackID[USB_MIDI_NUM_OUTPUTS];
} __packed;

// Element descriptor.
// See table 6-5 in the spec.
/* struct usb_midi_element_descriptor
{
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t bElementID;
        uint8_t bNrInputPins;
        {
            uint8_t baSourceID
                uint8_t baSourcePin}[NUM_ELEMENT_INPUTS];
        uint8_t bNrOutputPins;
        uint8_t bInTerminalLink;
        uint8_t bOutTerminalLink;
        uint8_t bElCapsSize;
        uint8_t bmElementCaps[ELEMENT_CAPS_COUNT];
        uint8_t iElement;
} __packed; */

// A complete set of descriptors for a USB MIDI device.
// Similar to the example in appendix B of the spec, but
// adapted to support any valid numbers of input and output ports.
struct usb_midi_config
{
        struct usb_device_descriptor dev;
        struct usb_cfg_descriptor cfg;
        struct usb_if_descriptor ac_if;
        struct usb_midi_ac_if_descriptor ac_cs_if;
        struct usb_if_descriptor ms_if;
        struct usb_midi_ms_if_descriptor ms_cs_if;
        struct usb_midi_in_jack_descriptor in_jacks_ext[USB_MIDI_NUM_INPUTS];
        struct usb_midi_in_jack_descriptor in_jacks_emb[USB_MIDI_NUM_OUTPUTS];
        struct usb_midi_out_jack_descriptor out_jacks_emb[USB_MIDI_NUM_INPUTS];
        struct usb_midi_out_jack_descriptor out_jacks_ext[USB_MIDI_NUM_OUTPUTS];
        struct usb_ep_descriptor out_ep;
        struct usb_midi_bulk_out_ep_descriptor out_cs_ep;
        struct usb_ep_descriptor in_ep;
        struct usb_midi_bulk_in_ep_descriptor in_cs_ep;
} __packed;

#endif