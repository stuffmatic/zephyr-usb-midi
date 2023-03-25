#ifndef ZEPHYR_USB_MIDI_TYPES_H_
#define ZEPHYR_USB_MIDI_TYPES_H_

#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>

/** 
 * MS (MIDI streaming) Class-Specific Interface Descriptor Subtypes. 
 * See table A.1 in the spec. 
 */
enum usb_midi_if_desc_subtype {
	USB_MIDI_IF_DESC_UNDEFINED =     0x00,
	USB_MIDI_IF_DESC_MS_HEADER =     0x01,
	USB_MIDI_IF_DESC_MIDI_IN_JACK =  0x02,
	USB_MIDI_IF_DESC_MIDI_OUT_JACK = 0x03,
	USB_MIDI_IF_DESC_ELEMENT =       0x04
};

/** 
 * MS Class-Specific Endpoint Descriptor Subtypes. 
 * See table A.2 in the spec. 
 */
enum usb_midi_ep_desc_subtype {
	USB_MIDI_EP_DESC_UNDEFINED =  0x00,
	USB_MIDI_EP_DESC_MS_GENERAL = 0x01
};

/** 
 * MS MIDI IN and OUT Jack types. 
 * See table A.3 in the spec. 
 */
enum usb_midi_jack_type {
	USB_MIDI_JACK_TYPE_UNDEFINED = 0x00,
	USB_MIDI_JACK_TYPE_EMBEDDED =  0x01,
	USB_MIDI_JACK_TYPE_EXTERNAL =  0x02
};

#define USB_MIDI_AUDIO_INTERFACE_CLASS            0x01
#define USB_MIDI_MIDISTREAMING_INTERFACE_SUBCLASS 0x03
#define USB_MIDI_AUDIOCONTROL_INTERFACE_SUBCLASS  0x01

/**
 * USB MIDI input pin.
 */
struct usb_midi_input_pin {
	uint8_t baSourceID;
	uint8_t baSourcePin;
} __packed;

/** 
 * Class-specific AC (audio control) Interface Descriptor. 
 */
struct usb_midi_ac_if_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint16_t bcdADC;
	uint16_t wTotalLength;
	uint8_t bInCollection;
	uint8_t baInterfaceNr;
} __packed;

/** 
 * Class-Specific MS Interface Header Descriptor. 
 * See table 6.2 in the spec. 
 */
struct usb_midi_ms_if_descriptor {
	/** Size of this descriptor, in bytes */
	uint8_t bLength;
	/** CS_INTERFACE descriptor type */
	uint8_t bDescriptorType;
	/** MS_HEADER descriptor subtype. */
	uint8_t bDescriptorSubtype;
	/** 
	 * MIDIStreaming SubClass Specification Release Number in 
	 * Binary-Coded Decimal. Currently 01.00. 
	 */
	uint16_t BcdADC;
	/** 
	 * Total number of bytes returned for the class-specific 
	 * MIDIStreaming interface descriptor. Includes the combined 
	 * length of this descriptor header and all Jack and Element descriptors. 
	 */
	uint16_t wTotalLength;
} __packed;

/** 
 * MIDI IN Jack Descriptor. See table 6.3 in the spec. 
 */
struct usb_midi_in_jack_descriptor {
	/** Size of this descriptor, in bytes. */
	uint8_t bLength;
	/** CS_INTERFACE descriptor type. */
	uint8_t bDescriptorType;
	/** MIDI_IN_JACK descriptor subtype. */
	uint8_t bDescriptorSubtype;
	/** EMBEDDED or EXTERNAL */
	uint8_t bJackType;
	/** 
	 * Constant uniquely identifying the MIDI IN Jack within
	 * the USB-MIDI function. 
	 */
	uint8_t bJackID;
	/** Index of a string descriptor, describing the MIDI IN Jack. */
	uint8_t iJack;
} __packed;

/** 
 * MIDI OUT Jack Descriptor. See table 6.4 in the spec. 
 */
struct usb_midi_out_jack_descriptor {
	/** Size of this descriptor, in bytes: */
	uint8_t bLength;
	/** CS_INTERFACE descriptor type. */
	uint8_t bDescriptorType;
	/** MIDI_OUT_JACK descriptor subtype. */
	uint8_t bDescriptorSubtype;
	/** EMBEDDED or EXTERNAL */
	uint8_t bJackType;
	/** 
	 * Constant uniquely identifying the MIDI OUT Jack 
	 * within the USB-MIDI function. 
	 */
	uint8_t bJackID;
	/** 
	 * Number of Input Pins of this MIDI OUT Jack 
	 * (assumed to be 1 in this implementation). 
	 */
	uint8_t bNrInputPins;
	/** ID and source pin of the entity to which this jack is connected. */
	struct usb_midi_input_pin input_pin;
	/** Index of a string descriptor, describing the MIDI OUT Jack. */
	uint8_t iJack;
} __packed;

/**
 * The same as Zephyr's usb_ep_descriptor but with two additional fields
 * to match the USB MIDI spec.
 */
struct usb_ep_descriptor_padded {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
	/* The following two attributes were added to match the USB MIDI spec. */
	uint8_t bRefresh;
	uint8_t bSynchAddress;
} __packed;

/** 
 * Class-Specific MS Bulk Data Endpoint Descriptor 
 * corresponding to a MIDI output. See table 6-7 in the spec. 
 */
struct usb_midi_bulk_out_ep_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bNumEmbMIDIJack;
	uint8_t BaAssocJackID[CONFIG_USB_MIDI_NUM_INPUTS];
} __packed;

/** 
 * Class-Specific MS Bulk Data Endpoint Descriptor 
 * corresponding to a MIDI input. See table 6-7 in the spec. 
 */
struct usb_midi_bulk_in_ep_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bNumEmbMIDIJack;
	uint8_t BaAssocJackID[CONFIG_USB_MIDI_NUM_OUTPUTS];
} __packed;

#define USB_MIDI_ELEMENT_CAPS_COUNT 1

/** 
 * Element descriptor. See table 6-5 in the spec. 
 */
struct usb_midi_element_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubtype;
	uint8_t bElementID;
	uint8_t bNrInputPins;

	struct usb_midi_input_pin input_pins[CONFIG_USB_MIDI_NUM_INPUTS];
	uint8_t bNrOutputPins;
	uint8_t bInTerminalLink;
	uint8_t bOutTerminalLink;
	uint8_t bElCapsSize;
	uint8_t bmElementCaps[USB_MIDI_ELEMENT_CAPS_COUNT];
	uint8_t iElement;
} __packed;

/** 
 * A complete set of descriptors for a USB MIDI device without physical jacks. 
 */
struct usb_midi_config {
	struct usb_if_descriptor ac_if;
	struct usb_midi_ac_if_descriptor ac_cs_if;
	struct usb_if_descriptor ms_if;
	struct usb_midi_ms_if_descriptor ms_cs_if;
	struct usb_midi_in_jack_descriptor in_jacks_emb[CONFIG_USB_MIDI_NUM_INPUTS];
	struct usb_midi_out_jack_descriptor out_jacks_emb[CONFIG_USB_MIDI_NUM_OUTPUTS];
	struct usb_midi_element_descriptor element;
	struct usb_ep_descriptor_padded out_ep;
	struct usb_midi_bulk_out_ep_descriptor out_cs_ep;
	struct usb_ep_descriptor_padded in_ep;
	struct usb_midi_bulk_in_ep_descriptor in_cs_ep;
} __packed;

#endif