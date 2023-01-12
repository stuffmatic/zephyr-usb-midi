#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>
#include <zephyr/logging/log.h>
#include "usb_midi.h"
#include "usb_midi_internal.h"
#include "usb_midi_packet.h"

/* Require at least one jack */
BUILD_ASSERT((USB_MIDI_NUM_INPUTS + USB_MIDI_NUM_OUTPUTS > 0), "USB MIDI device must have more than 0 jacks");

#ifdef CONFIG_USB_MIDI_USE_CUSTOM_JACK_NAMES

#define OUTPUT_JACK_STRING_DESCR_IDX(jack_idx) (4 + jack_idx)
#define INPUT_JACK_STRING_DESCR_IDX(jack_idx) (4 + jack_idx + USB_MIDI_NUM_OUTPUTS)

/* Define struct jack_string_descriptors holding all jack string descriptors. */
#define OUTPUT_JACK_STRING_DESCR(jack_number, _)                                                       \
	struct output_jack_##jack_number##_string_descr_type                                           \
	{                                                                                              \
		uint8_t bLength;                                                                       \
		uint8_t bDescriptorType;                                                               \
		uint8_t bString[USB_BSTRING_LENGTH(CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME)]; \
	} __packed output_jack_##jack_number##_string_descr;

#define INPUT_JACK_STRING_DESCR(jack_number, _)                                                       \
	struct input_jack_##jack_number##_string_descr_type                                           \
	{                                                                                             \
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
#define INIT_INPUT_JACK_STRING_DESCR(jack_number, _)                                                  \
	.input_jack_##jack_number##_string_descr = {                                                  \
	    .bLength = USB_STRING_DESCRIPTOR_LENGTH(CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME), \
	    .bDescriptorType = USB_DESC_STRING,                                                       \
	    .bString = CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME},

#define INIT_OUTPUT_JACK_STRING_DESCR(jack_number, _)                                                  \
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

#define INIT_AC_CS_IF                                                     \
	{                                                                 \
		.bLength = sizeof(struct usb_midi_ac_if_descriptor),      \
		.bDescriptorType = USB_DESC_CS_INTERFACE,                 \
		.bDescriptorSubtype = 0x01,                               \
		.bcdADC = 0x0100,                                         \
		.wTotalLength = sizeof(struct usb_midi_ac_if_descriptor), \
		.bInCollection = 0x01,                                    \
		.baInterfaceNr = 0x01                                     \
	}

#define INIT_MS_IF                                                   \
	{                                                            \
		.bLength = sizeof(struct usb_if_descriptor),         \
		.bDescriptorType = USB_DESC_INTERFACE,               \
		.bInterfaceNumber = 0x01,                            \
		.bAlternateSetting = 0x00,                           \
		.bNumEndpoints = 2,                                  \
		.bInterfaceClass = 0x01,	/* TODO: Constant */ \
		    .bInterfaceSubClass = 0x03, /* TODO: Constant */ \
		    .bInterfaceProtocol = 0x00,                      \
		.iInterface = 0x00                                   \
	}

#define INIT_MS_CS_IF(total_length)                                  \
	{                                                            \
		.bLength = sizeof(struct usb_midi_ms_if_descriptor), \
		.bDescriptorType = USB_DESC_CS_INTERFACE,            \
		.bDescriptorSubtype = 0x01,                          \
		.BcdADC = 0x0100,                                    \
		.wTotalLength = total_length                         \
	}

#define INIT_IN_JACK(idx, idx_offset)                                  \
	{                                                              \
		.bLength = sizeof(struct usb_midi_in_jack_descriptor), \
		.bDescriptorType = USB_DESC_CS_INTERFACE,              \
		.bDescriptorSubtype = USB_MIDI_IF_DESC_MIDI_IN_JACK,   \
		.bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,              \
		.bJackID = 1 + idx + idx_offset,                       \
		.iJack = INPUT_JACK_STRING_DESCR_IDX(idx),             \
	}

#define INIT_OUT_JACK(idx, jack_id_idx_offset)                          \
	{                                                               \
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

#define INIT_OUT_EP                                                 \
	{                                                           \
		.bLength = sizeof(struct usb_ep_descriptor_padded), \
		.bDescriptorType = USB_DESC_ENDPOINT,               \
		.bEndpointAddress = 0x01,                           \
		.bmAttributes = 0x02,                               \
		.wMaxPacketSize = 0x0040,                           \
		.bInterval = 0x00,                                  \
		.bRefresh = 0x00,                                   \
		.bSynchAddress = 0x00,                              \
	}

#define INIT_IN_EP                                                  \
	{                                                           \
		.bLength = sizeof(struct usb_ep_descriptor_padded), \
		.bDescriptorType = USB_DESC_ENDPOINT,               \
		.bEndpointAddress = 0x81,                           \
		.bmAttributes = 0x02,                               \
		.wMaxPacketSize = 0x0040,                           \
		.bInterval = 0x00,                                  \
		.bRefresh = 0x00,                                   \
		.bSynchAddress = 0x00,                              \
	}

#define INIT_DEVICE_DESC                                           \
	{                                                          \
		.bLength = sizeof(struct usb_device_descriptor),   \
		.bDescriptorType = USB_DESC_DEVICE,                \
		.bcdUSB = 0x0110,                                  \
		.bDeviceClass = 0x00,                              \
		.bDeviceSubClass = 0x00,                           \
		.bDeviceProtocol = 0x00,                           \
		.bMaxPacketSize0 = 64,	  /* 0x08 */               \
		    .idVendor = 0x2FE3,	  /* TODO: set properly */ \
		    .idProduct = 0x1,	  /* TODO: set properly */ \
		    .bcdDevice = 0x0,	  /* TODO: set properly */ \
		    .iManufacturer = 0x1, /* TODO: set properly */ \
		    .iProduct = 0x2,	  /* TODO: set properly */ \
		    .iSerialNumber = 0x0, /* TODO: set properly */ \
		    .bNumConfigurations = 0x1                      \
	}

#define INIT_CFG_DESC(total_length)                             \
	{                                                       \
		.bLength = sizeof(struct usb_cfg_descriptor),   \
		.bDescriptorType = USB_DESC_CONFIGURATION,      \
		.wTotalLength = total_length,                   \
		.bNumInterfaces = 0x02,                         \
		.bConfigurationValue = 0x01,                    \
		.iConfiguration = 0x00,                         \
		.bmAttributes = 0x80,  /* TODO: set properly */ \
		    .bMaxPower = 0x32, /* TODO: set properly */ \
	}

#define INIT_AC_IF                                                   \
	{                                                            \
		.bLength = sizeof(struct usb_if_descriptor),         \
		.bDescriptorType = USB_DESC_INTERFACE,               \
		.bInterfaceNumber = 0,                               \
		.bAlternateSetting = 0,                              \
		.bNumEndpoints = 0,                                  \
		.bInterfaceClass = 0x01,	/* TODO: Constant */ \
		    .bInterfaceSubClass = 0x01, /* TODO: Constant */ \
		    .bInterfaceProtocol = 0x00,                      \
		.iInterface = 0x00                                   \
	}

#define ELEMENT_ID 0xf0
#define IDX_WITH_OFFSET(index, offset) (index + offset)
#define INIT_INPUT_PIN(index, offset)                \
	{                                       \
		.baSourceID = (index + offset), \
		.baSourcePin = 1                \
	}

#define INIT_ELEMENT                                                                                                                                    \
	{                                                                                                                                               \
		.bLength = sizeof(struct usb_midi_element_descriptor),                                                                                  \
		.bDescriptorType = USB_DESC_CS_INTERFACE,                                                                                               \
		.bDescriptorSubtype = USB_MIDI_IF_DESC_ELEMENT,                                                                                         \
		.bElementID = ELEMENT_ID,                                                                                                               \
		.bNrInputPins = USB_MIDI_NUM_INPUTS,                                                                                                    \
		.input_pins = {                                                                                                                         \
		    LISTIFY(USB_MIDI_NUM_INPUTS, INIT_INPUT_PIN, (, ), 1)},                                                                                  \
		.bNrOutputPins = USB_MIDI_NUM_OUTPUTS, .bInTerminalLink = 0, .bOutTerminalLink = 0, .bElCapsSize = 1, .bmElementCaps = 1, .iElement = 0 \
	}

/* Value for the wTotalLength field of the class-specific MS Interface Descriptor,
   i.e the total number of bytes following the class-specific MS Interface Descriptor. */
#define MIDI_MS_IF_DESC_TOTAL_SIZE                                               \
	(                                                                        \
	    sizeof(struct usb_midi_in_jack_descriptor) * USB_MIDI_NUM_INPUTS +   \
	    sizeof(struct usb_midi_out_jack_descriptor) * USB_MIDI_NUM_OUTPUTS + \
	    sizeof(struct usb_midi_element_descriptor) +                         \
	    sizeof(struct usb_ep_descriptor_padded) +                            \
	    sizeof(struct usb_midi_bulk_out_ep_descriptor) +                     \
	    sizeof(struct usb_ep_descriptor_padded) +                            \
	    sizeof(struct usb_midi_bulk_in_ep_descriptor))

/* Value for the wTotalLength field of the Configuration Descriptor. */
#define CFG_TOTAL_LENGTH                               \
	(                                              \
	    sizeof(struct usb_cfg_descriptor) +        \
	    sizeof(struct usb_if_descriptor) +         \
	    sizeof(struct usb_midi_ac_if_descriptor) + \
	    sizeof(struct usb_if_descriptor) +         \
	    sizeof(struct usb_midi_ms_if_descriptor) + \
	    MIDI_MS_IF_DESC_TOTAL_SIZE)

/* Descriptor size sanity check */
BUILD_ASSERT(sizeof(struct usb_midi_config) == (CFG_TOTAL_LENGTH + sizeof(struct usb_device_descriptor)), "");

USBD_DEVICE_DESCR_DEFINE(primary) // TODO: why primary?
struct usb_midi_config usb_midi_config_data = {
    .dev = INIT_DEVICE_DESC,
    .cfg = INIT_CFG_DESC(CFG_TOTAL_LENGTH),
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
    .out_cs_ep = {.bLength = sizeof(struct usb_midi_bulk_out_ep_descriptor), .bDescriptorType = USB_DESC_CS_ENDPOINT, .bDescriptorSubtype = 0x01, .bNumEmbMIDIJack = USB_MIDI_NUM_INPUTS, .BaAssocJackID = {LISTIFY(USB_MIDI_NUM_INPUTS, IDX_WITH_OFFSET, (, ), 1 + USB_MIDI_NUM_OUTPUTS)}
	}
};

static bool usb_midi_is_available = false;
static struct usb_midi_handlers handlers = {
    .available_cb = NULL,
    .rx_cb = NULL};

static void log_packet(struct usb_midi_packet *packet)
{
	printk("USB MIDI packet %02x %02x %02x %02x | cable %02x | CIN %01x | %d MIDI bytes\n",
	       packet->bytes[0], packet->bytes[1], packet->bytes[2], packet->bytes[3],
	       packet->cable_num, packet->cin, packet->num_midi_bytes);
}

void usb_midi_register_handlers(struct usb_midi_handlers *h)
{
	handlers.available_cb = h->available_cb;
	handlers.rx_cb = h->rx_cb;
}

static void midi_out_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code
					   ep_status)
{
	uint8_t buf[4];
	uint32_t num_read_bytes = 0;
	usb_read(ep, buf, 4, &num_read_bytes);
	struct usb_midi_packet packet;

	uint32_t parse_rc = usb_midi_packet_from_usb_bytes(buf, &packet);
	if (parse_rc)
	{
			// WARN ignored message
	}
	else if (handlers.rx_cb)
	{
		handlers.rx_cb(packet.cable_num, &packet.bytes[1], packet.num_midi_bytes);
	}
	// printk("rx ");
	// log_packet(&packet);
	// usb_read(ep, NULL, 0, &bytes_to_read);
	// LOG_DBG("ep 0x%x, bytes to read %d ", ep, bytes_to_read);
	// usb_read(ep, loopback_buf, bytes_to_read, NULL);
}

static void midi_in_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code
					  ep_status)
{
	// LOG_DBG("midi_out_cb ep %d, ep_status %d\n", ep, ep_status);
	/* if (usb_write(ep, loopback_buf, CONFIG_LOOPBACK_BULK_EP_MPS,
			NULL)) {
		LOG_DBG("ep 0x%x", ep);
	} */
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
		// printk("USB_DC_ERROR\n");
		break;
	/** USB reset */
	case USB_DC_RESET:
		// printk("USB_DC_RESET\n");
		break;
	/** USB connection established, hardware enumeration is completed */
	case USB_DC_CONNECTED:
		// printk("USB_DC_CONNECTED\n");
		break;
	/** USB configuration done */
	case USB_DC_CONFIGURED:
		if (!usb_midi_is_available && handlers.available_cb)
		{
			handlers.available_cb(true);
		}
		usb_midi_is_available = true;
		// printk("USB_DC_CONFIGURED\n");
		break;
	/** USB connection lost */
	case USB_DC_DISCONNECTED:
		// printk("USB_DC_DISCONNECTED\n");
		break;
	/** USB connection suspended by the HOST */
	case USB_DC_SUSPEND:
		if (usb_midi_is_available && handlers.available_cb)
		{
			handlers.available_cb(false);
		}
		usb_midi_is_available = false;
		// printk("USB_DC_SUSPEND\n");
		break;
	/** USB connection resumed by the HOST */
	case USB_DC_RESUME:
		// printk("USB_DC_RESUME\n");
		break;
	/** USB interface selected */
	case USB_DC_INTERFACE:
		// printk("USB_DC_INTERFACE\n");
		break;
	/** Set Feature ENDPOINT_HALT received */
	case USB_DC_SET_HALT:
		// printk("USB_DC_SET_HALT\n");
		break;
	/** Clear Feature ENDPOINT_HALT received */
	case USB_DC_CLEAR_HALT:
		// printk("USB_DC_CLEAR_HALT\n");
		break;
	/** Start of Frame received */
	case USB_DC_SOF:
		// printk("USB_DC_SOF\n");
		break;
	/** Initial USB connection status */
	case USB_DC_UNKNOWN:
		// printk("USB_DC_UNKNOWN\n");
		break;
	}
}

uint32_t usb_midi_tx(uint8_t cable_number, uint8_t* midi_bytes)
{
	if (cable_number < USB_MIDI_NUM_OUTPUTS)
	{
		return 0;
	}

	struct usb_midi_packet packet;
	uint32_t parse_rc = usb_midi_packet_from_midi_bytes(midi_bytes, cable_number, &packet);
	// printk("tx ");
	// log_packet(&packet);
	// TODO: report error
	uint32_t num_written_bytes = 0;
	usb_write(0x81, packet.bytes, 4, &num_written_bytes);
	return num_written_bytes;
}

USBD_DEFINE_CFG_DATA(usb_midi_config) = {
    .usb_device_description = &usb_midi_config_data,
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