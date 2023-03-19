#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>
#include <usb_midi/usb_midi.h>
#include <usb_midi/usb_midi_packet.h>
#include "usb_midi_internal.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);

/* Require at least one jack */
BUILD_ASSERT((USB_MIDI_NUM_INPUTS + USB_MIDI_NUM_OUTPUTS > 0), "USB MIDI device must have more than 0 jacks");

#ifdef CONFIG_USB_MIDI_USE_CUSTOM_JACK_NAMES

#define OUTPUT_JACK_STRING_DESCR_IDX(jack_idx) (4 + jack_idx)
#define INPUT_JACK_STRING_DESCR_IDX(jack_idx) (4 + jack_idx + USB_MIDI_NUM_OUTPUTS)

/* Define struct jack_string_descriptors holding all jack string descriptors. */
#define OUTPUT_JACK_STRING_DESCR(jack_number, _)                                           \
	struct output_jack_##jack_number##_string_descr_type                                     \
	{                                                                                        \
		uint8_t bLength;                                                                       \
		uint8_t bDescriptorType;                                                               \
		uint8_t bString[USB_BSTRING_LENGTH(CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME)]; \
	} __packed output_jack_##jack_number##_string_descr;

#define INPUT_JACK_STRING_DESCR(jack_number, _)                                           \
	struct input_jack_##jack_number##_string_descr_type                                     \
	{                                                                                       \
		uint8_t bLength;                                                                      \
		uint8_t bDescriptorType;                                                              \
		uint8_t bString[USB_BSTRING_LENGTH(CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME)]; \
	} __packed input_jack_##jack_number##_string_descr;

struct jack_string_descriptors
{
	LISTIFY(USB_MIDI_NUM_OUTPUTS, OUTPUT_JACK_STRING_DESCR, ())
	LISTIFY(USB_MIDI_NUM_INPUTS, INPUT_JACK_STRING_DESCR, ())
} _packed;

/* statically initialize a struct jack_string_descriptors */
#define INIT_INPUT_JACK_STRING_DESCR(jack_number, _)                                            \
	.input_jack_##jack_number##_string_descr = {                                                  \
			.bLength = USB_STRING_DESCRIPTOR_LENGTH(CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME), \
			.bDescriptorType = USB_DESC_STRING,                                                       \
			.bString = CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME},

#define INIT_OUTPUT_JACK_STRING_DESCR(jack_number, _)                                            \
	.output_jack_##jack_number##_string_descr = {                                                  \
			.bLength = USB_STRING_DESCRIPTOR_LENGTH(CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME), \
			.bDescriptorType = USB_DESC_STRING,                                                        \
			.bString = CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME},

USBD_STRING_DESCR_USER_DEFINE(primary)
struct jack_string_descriptors jack_string_desc = {
		LISTIFY(USB_MIDI_NUM_OUTPUTS, INIT_OUTPUT_JACK_STRING_DESCR, ())
				LISTIFY(USB_MIDI_NUM_INPUTS, INIT_INPUT_JACK_STRING_DESCR, ())};
#else
/* No jack string descriptors by default  */
#define INPUT_JACK_STRING_DESCR_IDX(jack_idx) 0
#define OUTPUT_JACK_STRING_DESCR_IDX(jack_idx) 0
#endif /* CONFIG_USB_MIDI_USE_CUSTOM_JACK_NAMES */

#define INIT_AC_CS_IF                                         \
	{                                                           \
		.bLength = sizeof(struct usb_midi_ac_if_descriptor),      \
		.bDescriptorType = USB_DESC_CS_INTERFACE,                 \
		.bDescriptorSubtype = 0x01,                               \
		.bcdADC = 0x0100,                                         \
		.wTotalLength = sizeof(struct usb_midi_ac_if_descriptor), \
		.bInCollection = 0x01,                                    \
		.baInterfaceNr = 0x01                                     \
	}

#define INIT_MS_IF                                       \
	{                                                      \
		.bLength = sizeof(struct usb_if_descriptor),         \
		.bDescriptorType = USB_DESC_INTERFACE,               \
		.bInterfaceNumber = 0x01,                            \
		.bAlternateSetting = 0x00,                           \
		.bNumEndpoints = 2,                                  \
		.bInterfaceClass = 0x01,				/* TODO: Constant */ \
				.bInterfaceSubClass = 0x03, /* TODO: Constant */ \
				.bInterfaceProtocol = 0x00,                      \
		.iInterface = 0x00                                   \
	}

#define INIT_MS_CS_IF(total_length)                      \
	{                                                      \
		.bLength = sizeof(struct usb_midi_ms_if_descriptor), \
		.bDescriptorType = USB_DESC_CS_INTERFACE,            \
		.bDescriptorSubtype = 0x01,                          \
		.BcdADC = 0x0100,                                    \
		.wTotalLength = total_length                         \
	}

#define INIT_IN_JACK(idx, idx_offset)                      \
	{                                                        \
		.bLength = sizeof(struct usb_midi_in_jack_descriptor), \
		.bDescriptorType = USB_DESC_CS_INTERFACE,              \
		.bDescriptorSubtype = USB_MIDI_IF_DESC_MIDI_IN_JACK,   \
		.bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,              \
		.bJackID = 1 + idx + idx_offset,                       \
		.iJack = INPUT_JACK_STRING_DESCR_IDX(idx),             \
	}

#define INIT_OUT_JACK(idx, jack_id_idx_offset)              \
	{                                                         \
		.bLength = sizeof(struct usb_midi_out_jack_descriptor), \
		.bDescriptorType = USB_DESC_CS_INTERFACE,               \
		.bDescriptorSubtype = USB_MIDI_IF_DESC_MIDI_OUT_JACK,   \
		.bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,               \
		.bJackID = 1 + idx + jack_id_idx_offset,                \
		.bNrInputPins = 0x01,                                   \
		.input_pin = {                                          \
				.baSourceID = ELEMENT_ID,                           \
				.baSourcePin = 1 + idx,                             \
		},                                                      \
		.iJack = OUTPUT_JACK_STRING_DESCR_IDX(idx)              \
	}

#define INIT_OUT_EP                                     \
	{                                                     \
		.bLength = sizeof(struct usb_ep_descriptor_padded), \
		.bDescriptorType = USB_DESC_ENDPOINT,               \
		.bEndpointAddress = 0x01,                           \
		.bmAttributes = 0x02,                               \
		.wMaxPacketSize = 0x0040,                           \
		.bInterval = 0x00,                                  \
		.bRefresh = 0x00,                                   \
		.bSynchAddress = 0x00,                              \
	}

#define INIT_IN_EP                                      \
	{                                                     \
		.bLength = sizeof(struct usb_ep_descriptor_padded), \
		.bDescriptorType = USB_DESC_ENDPOINT,               \
		.bEndpointAddress = 0x81,                           \
		.bmAttributes = 0x02,                               \
		.wMaxPacketSize = 0x0040,                           \
		.bInterval = 0x00,                                  \
		.bRefresh = 0x00,                                   \
		.bSynchAddress = 0x00,                              \
	}

#define INIT_AC_IF                                       \
	{                                                      \
		.bLength = sizeof(struct usb_if_descriptor),         \
		.bDescriptorType = USB_DESC_INTERFACE,               \
		.bInterfaceNumber = 0,                               \
		.bAlternateSetting = 0,                              \
		.bNumEndpoints = 0,                                  \
		.bInterfaceClass = 0x01,				/* TODO: Constant */ \
				.bInterfaceSubClass = 0x01, /* TODO: Constant */ \
				.bInterfaceProtocol = 0x00,                      \
		.iInterface = 0x00                                   \
	}

#define ELEMENT_ID 0xf0
#define IDX_WITH_OFFSET(index, offset) (index + offset)
#define INIT_INPUT_PIN(index, offset) \
	{                                   \
		.baSourceID = (index + offset),   \
		.baSourcePin = 1                  \
	}

#define INIT_ELEMENT                                                                                                                        \
	{                                                                                                                                         \
		.bLength = sizeof(struct usb_midi_element_descriptor),                                                                                  \
		.bDescriptorType = USB_DESC_CS_INTERFACE,                                                                                               \
		.bDescriptorSubtype = USB_MIDI_IF_DESC_ELEMENT,                                                                                         \
		.bElementID = ELEMENT_ID,                                                                                                               \
		.bNrInputPins = USB_MIDI_NUM_INPUTS,                                                                                                    \
		.input_pins = {                                                                                                                         \
				LISTIFY(USB_MIDI_NUM_INPUTS, INIT_INPUT_PIN, (, ), 1)},                                                                             \
		.bNrOutputPins = USB_MIDI_NUM_OUTPUTS, .bInTerminalLink = 0, .bOutTerminalLink = 0, .bElCapsSize = 1, .bmElementCaps = 1, .iElement = 0 \
	}

/* Value for the wTotalLength field of the class-specific MS Interface Descriptor,
	 i.e the total number of bytes following the class-specific MS Interface Descriptor. */
#define MIDI_MS_IF_DESC_TOTAL_SIZE                                         \
	(                                                                        \
			sizeof(struct usb_midi_in_jack_descriptor) * USB_MIDI_NUM_INPUTS +   \
			sizeof(struct usb_midi_out_jack_descriptor) * USB_MIDI_NUM_OUTPUTS + \
			sizeof(struct usb_midi_element_descriptor) +                         \
			sizeof(struct usb_ep_descriptor_padded) +                            \
			sizeof(struct usb_midi_bulk_out_ep_descriptor) +                     \
			sizeof(struct usb_ep_descriptor_padded) +                            \
			sizeof(struct usb_midi_bulk_in_ep_descriptor))

USBD_CLASS_DESCR_DEFINE(primary, 0) // TODO: why primary?
struct usb_midi_config usb_midi_config_data = {
		.ac_if = INIT_AC_IF,
		.ac_cs_if = INIT_AC_CS_IF,
		.ms_if = INIT_MS_IF,
		.ms_cs_if = INIT_MS_CS_IF(MIDI_MS_IF_DESC_TOTAL_SIZE),
		.out_jacks_emb = {
				LISTIFY(USB_MIDI_NUM_OUTPUTS, INIT_OUT_JACK, (, ), 0)},
		.in_jacks_emb = {LISTIFY(USB_MIDI_NUM_INPUTS, INIT_IN_JACK, (, ), USB_MIDI_NUM_OUTPUTS)},
		.element = INIT_ELEMENT,
		.in_ep = INIT_IN_EP,
		.in_cs_ep = {.bLength = sizeof(struct usb_midi_bulk_in_ep_descriptor), .bDescriptorType = USB_DESC_CS_ENDPOINT, .bDescriptorSubtype = 0x01, .bNumEmbMIDIJack = USB_MIDI_NUM_OUTPUTS, .BaAssocJackID = {LISTIFY(USB_MIDI_NUM_OUTPUTS, IDX_WITH_OFFSET, (, ), 1)}},
		.out_ep = INIT_OUT_EP,
		.out_cs_ep = {.bLength = sizeof(struct usb_midi_bulk_out_ep_descriptor), .bDescriptorType = USB_DESC_CS_ENDPOINT, .bDescriptorSubtype = 0x01, .bNumEmbMIDIJack = USB_MIDI_NUM_INPUTS, .BaAssocJackID = {LISTIFY(USB_MIDI_NUM_INPUTS, IDX_WITH_OFFSET, (, ), 1 + USB_MIDI_NUM_OUTPUTS)}}};

static bool usb_midi_is_available = false;
static struct usb_midi_cb_t user_callbacks = {
		.available_cb = NULL,
		.midi_message_cb = NULL,
		.tx_done_cb = NULL,
		.sysex_data_cb = NULL,
		.sysex_end_cb = NULL,
		.sysex_start_cb = NULL};

#define LOG_DBG_PACKET(packet) LOG_DBG("USB MIDI packet %02x %02x %02x %02x | cable %02x | CIN %01x | %d MIDI bytes", \
				 packet.bytes[0], packet.bytes[1], packet.bytes[2], packet.bytes[3], \
				 packet.cable_num, packet.cin, packet.num_midi_bytes)

void usb_midi_register_callbacks(struct usb_midi_cb_t *cb)
{
	user_callbacks.available_cb = cb->available_cb;
	user_callbacks.midi_message_cb = cb->midi_message_cb;
	user_callbacks.tx_done_cb = cb->tx_done_cb;
	user_callbacks.sysex_start_cb = cb->sysex_start_cb;
	user_callbacks.sysex_data_cb = cb->sysex_data_cb;
	user_callbacks.sysex_end_cb = cb->sysex_end_cb;
}

static void midi_out_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code
																					 ep_status)
{
	uint8_t buf[4];
	uint32_t num_read_bytes = 1;
	while (num_read_bytes > 0)
	{
		int read_rc = usb_read(ep, buf, 4, &num_read_bytes);
		if (num_read_bytes == 0) {
			break;
		}
		struct usb_midi_packet_t packet;
		enum usb_midi_error_t error = usb_midi_packet_from_usb_bytes(buf, &packet);
		
		if (error != USB_MIDI_SUCCESS)
		{
			LOG_WRN("Packet parsing failed with error %d", error);
		}
		else
		{
			LOG_DBG_PACKET(packet);
			struct usb_midi_parse_cb_t parse_cb = {
					.message_cb = user_callbacks.midi_message_cb,
					.sysex_data_cb = user_callbacks.sysex_data_cb,
					.sysex_end_cb = user_callbacks.sysex_end_cb,
					.sysex_start_cb = user_callbacks.sysex_start_cb};
			error = usb_midi_parse_packet(packet.bytes, &parse_cb);
			if (error != USB_MIDI_SUCCESS) {
				LOG_ERR("Failed to parse packet");
			}
		}
	}
}

static void midi_in_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code
																					ep_status)
{
	if (ep_status == USB_DC_EP_DATA_IN && user_callbacks.tx_done_cb) {
		user_callbacks.tx_done_cb();
	}
}

static struct usb_ep_cfg_data midi_ep_cfg[] = {
		{
				.ep_cb = midi_in_ep_cb,
				.ep_addr = 0x81,
		},
		{
				.ep_cb = midi_out_ep_cb,
				.ep_addr = 0x01,
		}};

void usb_status_callback(struct usb_cfg_data *cfg,
												 enum usb_dc_status_code cb_status,
												 const uint8_t *param)
{
	switch (cb_status)
	{
	/** USB error reported by the controller */
	case USB_DC_ERROR:
		LOG_DBG("USB_DC_ERROR");
		break;
	/** USB reset */
	case USB_DC_RESET:
		LOG_DBG("USB_DC_RESET");
		break;
	/** USB connection established, hardware enumeration is completed */
	case USB_DC_CONNECTED:
		LOG_DBG("USB_DC_CONNECTED");
		break;
	/** USB configuration done */
	case USB_DC_CONFIGURED:
		LOG_DBG("USB_DC_CONFIGURED");
		if (!usb_midi_is_available && user_callbacks.available_cb)
		{
			LOG_INFO("USB MIDI device is available");
			user_callbacks.available_cb(true);
		}
		usb_midi_is_available = true;
		break;
	/** USB connection lost */
	case USB_DC_DISCONNECTED:
		LOG_DBG("USB_DC_DISCONNECTED");
		break;
	/** USB connection suspended by the HOST */
	case USB_DC_SUSPEND:
		LOG_DBG("USB_DC_SUSPEND");
		if (usb_midi_is_available && user_callbacks.available_cb)
		{
			LOG_INFO("USB MIDI device is unavailable");
			user_callbacks.available_cb(false);
		}
		usb_midi_is_available = false;
		break;
	/** USB connection resumed by the HOST */
	case USB_DC_RESUME:
		LOG_DBG("USB_DC_RESUME");
		break;
	/** USB interface selected */
	case USB_DC_INTERFACE:
		LOG_DBG("USB_DC_INTERFACE");
		break;
	/** Set Feature ENDPOINT_HALT received */
	case USB_DC_SET_HALT:
		LOG_DBG("USB_DC_SET_HALT");
		break;
	/** Clear Feature ENDPOINT_HALT received */
	case USB_DC_CLEAR_HALT:
		LOG_DBG("USB_DC_CLEAR_HALT");
		break;
	/** Start of Frame received */
	case USB_DC_SOF:
		LOG_DBG("USB_DC_SOF");
		break;
	/** Initial USB connection status */
	case USB_DC_UNKNOWN:
		LOG_DBG("USB_DC_UNKNOWN");
		break;
	}
}

int usb_midi_tx(uint8_t cable_number, uint8_t *midi_bytes)
{
	struct usb_midi_packet_t packet;
	enum usb_midi_error_t error = usb_midi_packet_from_midi_bytes(midi_bytes, cable_number, &packet);
	if (error != USB_MIDI_SUCCESS)
	{
		LOG_ERR("Building packet from MIDI bytes %02x %02x %02x failed with error %d", midi_bytes[0], midi_bytes[1], midi_bytes[2], error);
		return -EINVAL;
	}
	LOG_DBG_PACKET(packet);
	uint32_t num_written_bytes = 0;
	usb_write(0x81, packet.bytes, 4, &num_written_bytes);
	return num_written_bytes;
}

USBD_DEFINE_CFG_DATA(usb_midi_config) = {
		.usb_device_description = NULL,
		.interface_config = NULL,
		.interface_descriptor = &usb_midi_config_data.ac_if,
		.cb_usb_status = usb_status_callback,
		.interface = {
				.class_handler = NULL,
				.custom_handler = NULL,
				.vendor_handler = NULL,
		},
		.num_endpoints = ARRAY_SIZE(midi_ep_cfg),
		.endpoint = midi_ep_cfg,
};