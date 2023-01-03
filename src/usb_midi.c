#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>
#include "usb_midi.h"
#include "usb_midi_internal.h"

BUILD_ASSERT((USB_MIDI_NUM_INPUTS + USB_MIDI_NUM_OUTPUTS > 0), "USB MIDI device must have more than 0 ports");

#define INIT_AC_CS_IF(num_streaming_ifs, interface_nr)                    \
	{                                                                 \
		.bLength = sizeof(struct usb_midi_ac_if_descriptor),      \
		.bDescriptorType = USB_DESC_CS_INTERFACE,                 \
		.bDescriptorSubtype = 0x01,                               \
		.bcdADC = 0x0100,                                         \
		.wTotalLength = sizeof(struct usb_midi_ac_if_descriptor), \
		.bInCollection = num_streaming_ifs,                       \
		.baInterfaceNr = interface_nr                             \
	}

#define INIT_MS_CS_IF(total_length)                                  \
	{                                                            \
		.bLength = sizeof(struct usb_midi_ms_if_descriptor), \
		.bDescriptorType = USB_DESC_CS_INTERFACE,            \
		.bDescriptorSubtype = 0x01,                          \
		.BcdADC = 0x0100,                                    \
		.wTotalLength = total_length                         \
	}

#define INIT_IN_JACK(idx, idx_offset, jack_type)                     \
	{                                                              \
		.bLength = sizeof(struct usb_midi_in_jack_descriptor), \
		.bDescriptorType = USB_DESC_CS_INTERFACE,              \
		.bDescriptorSubtype = 0x02,                            \
		.bJackType = jack_type,                \
		.bJackID = 1 + idx + idx_offset,                           \
		.iJack = 0x00,                                         \
	}

#define INIT_OUT_JACK(idx, jack_id_idx_offset, source_id_idx_offset, jack_type) \
	{                                                                         \
		.bLength = sizeof(struct usb_midi_out_jack_descriptor),           \
		.bDescriptorType = USB_DESC_CS_INTERFACE,                         \
		.bDescriptorSubtype = 0x03,                                       \
		.bJackType = jack_type,                           \
		.bJackID = 1 + idx + jack_id_idx_offset,                              \
		.bNrInputPins = 0x01,                                             \
		.BaSourceID = 1 + idx + source_id_idx_offset,                         \
		.BaSourcePin = 0x01,                                              \
		.iJack = 0x00                                                     \
	}

#define JACK_ID(x, first_id) (x + first_id)
#define INIT_OUT_EP(num_embedded_jacks, embedded_in_jack_id)                           \
	{                                                                                 \
		.bLength = sizeof(struct usb_midi_bulk_out_ep_descriptor),                \
		.bDescriptorType = USB_DESC_CS_ENDPOINT,                                  \
		.bDescriptorSubtype = 0x01,                                               \
		.bNumEmbMIDIJack = num_embedded_jacks,                                 \
		.BaAssocJackID = {                                                        \
			LISTIFY(num_embedded_jacks, JACK_ID, (, ), embedded_in_jack_id) \
		}                                                                         \
	}

#define INIT_IN_EP(num_embedded_jacks, embedded_out_jack_id)                          \
	{                                                                                 \
		.bLength = sizeof(struct usb_midi_bulk_in_ep_descriptor),                 \
		.bDescriptorType = USB_DESC_CS_ENDPOINT,                                  \
		.bDescriptorSubtype = 0x01,                                               \
		.bNumEmbMIDIJack = num_embedded_jacks,                                \
		.BaAssocJackID = {                                                        \
			LISTIFY(num_embedded_jacks, JACK_ID, (, ), embedded_out_jack_id) \
		}                                                                         \
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

// The number of endpoints. Hardcoded to 2: one in and one out.
#define NUM_ENDPOINTS 2

// Value for the wTotalLength field of the class-specific MS Interface Descriptor,
// i.e the total number of bytes following the class-specific MS Interface Descriptor.
#define MIDI_MS_IF_DESC_TOTAL_SIZE                                               \
	(                                                                        \
	    sizeof(struct usb_midi_in_jack_descriptor) * USB_MIDI_NUM_INPUTS +   \
	    sizeof(struct usb_midi_out_jack_descriptor) * USB_MIDI_NUM_INPUTS +  \
	    sizeof(struct usb_midi_in_jack_descriptor) * USB_MIDI_NUM_OUTPUTS +  \
	    sizeof(struct usb_midi_out_jack_descriptor) * USB_MIDI_NUM_OUTPUTS + \
	    sizeof(struct usb_ep_descriptor) +                                   \
	    sizeof(struct usb_midi_bulk_out_ep_descriptor) +                     \
	    sizeof(struct usb_ep_descriptor) +                                   \
	    sizeof(struct usb_midi_bulk_in_ep_descriptor))

// Value for the wTotalLength field of the Configuration Descriptor.
#define CFG_TOTAL_LENGTH                               \
	(                                              \
	    sizeof(struct usb_cfg_descriptor) +        \
	    sizeof(struct usb_if_descriptor) +         \
	    sizeof(struct usb_midi_ac_if_descriptor) + \
	    sizeof(struct usb_if_descriptor) +         \
	    sizeof(struct usb_midi_ms_if_descriptor) + \
	    MIDI_MS_IF_DESC_TOTAL_SIZE)

BUILD_ASSERT(sizeof(struct usb_midi_config) == (CFG_TOTAL_LENGTH + sizeof(struct usb_device_descriptor)), "");

USBD_DEVICE_DESCR_DEFINE(primary)
struct usb_midi_config usb_midi_config_data = {
    .dev = INIT_DEVICE_DESC,
    .cfg = INIT_CFG_DESC(CFG_TOTAL_LENGTH),
    .ac_if = {
	.bLength = sizeof(struct usb_if_descriptor),
	.bDescriptorType = USB_DESC_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = 0x01,    // TODO: Constant
	.bInterfaceSubClass = 0x01, // TODO: Constant
	.bInterfaceProtocol = 0x00,
	.iInterface = 0x00},
    .ac_cs_if = INIT_AC_CS_IF(0x01, 0x01),
    .ms_if = {.bLength = sizeof(struct usb_if_descriptor), .bDescriptorType = USB_DESC_INTERFACE, .bInterfaceNumber = 0x01, .bAlternateSetting = 0x00, .bNumEndpoints = NUM_ENDPOINTS,
	      .bInterfaceClass = 0x01,	  // TODO: Constant
	      .bInterfaceSubClass = 0x03, // TODO: Constant
	      .bInterfaceProtocol = 0x00,
	      .iInterface = 0x00},
    .ms_cs_if = INIT_MS_CS_IF(MIDI_MS_IF_DESC_TOTAL_SIZE),
    .in_jacks_ext = {// 1..NUM_INPUTS
		     LISTIFY(USB_MIDI_NUM_INPUTS, INIT_IN_JACK, (, ), 0, USB_MIDI_JACK_TYPE_EXTERNAL)},
    .in_jacks_emb = {
	// 1..NUM_OUTPUTS + offs (=NUM_INPUTS)
	LISTIFY(USB_MIDI_NUM_OUTPUTS, INIT_IN_JACK, (, ), USB_MIDI_NUM_INPUTS, USB_MIDI_JACK_TYPE_EMBEDDED)
	// INIT_IN_JACK(0x02, 1),
	// INIT_IN_JACK(0x03, 1),
    },
    .out_jacks_emb = {
	// 1..NUM_INPUTS + offs (=NUM_INPUTS + NUM_OUTPUTS)
	LISTIFY(USB_MIDI_NUM_INPUTS, INIT_OUT_JACK, (, ), USB_MIDI_NUM_INPUTS + USB_MIDI_NUM_OUTPUTS, 0, USB_MIDI_JACK_TYPE_EMBEDDED)
	// INIT_OUT_JACK(0x04, 0x01, 1), // ref ex in i
    },
    .out_jacks_ext = {
	LISTIFY(USB_MIDI_NUM_OUTPUTS, INIT_OUT_JACK, (, ), USB_MIDI_NUM_INPUTS + USB_MIDI_NUM_OUTPUTS, USB_MIDI_NUM_INPUTS, USB_MIDI_JACK_TYPE_EXTERNAL)
	// 1..NUM_OUTPUTS
	// INIT_OUT_JACK(0x05, 0x05, 0),
	// INIT_OUT_JACK(0x06, 0x06, 0), // ref ex out i
    },
    .out_ep = {.bLength = sizeof(struct usb_ep_descriptor), .bDescriptorType = USB_DESC_ENDPOINT, .bEndpointAddress = 0x01, .bmAttributes = 0x02, .wMaxPacketSize = 0x0040, .bInterval = 0x00},
    .out_cs_ep = INIT_OUT_EP(USB_MIDI_NUM_INPUTS, USB_MIDI_NUM_INPUTS + USB_MIDI_NUM_OUTPUTS + 1),
    .in_ep = {.bLength = sizeof(struct usb_ep_descriptor), .bDescriptorType = USB_DESC_ENDPOINT, .bEndpointAddress = 0x81, .bmAttributes = 0x02, .wMaxPacketSize = 0x0040, .bInterval = 0x00},
    .in_cs_ep = INIT_IN_EP(USB_MIDI_NUM_OUTPUTS, USB_MIDI_NUM_INPUTS + 1),
};

static bool usb_midi_is_enabled = false;
static struct usb_midi_handlers handlers = {
    .enabled_cb = NULL,
    .rx_cb = NULL};

static void log_packet(struct usb_midi_packet *packet)
{
	printk("USB MIDI packet %02x %02x %02x %02x | cable %02x | CIN %01x | %d MIDI bytes\n",
	       packet->bytes[0], packet->bytes[1], packet->bytes[2], packet->bytes[3],
	       packet->cable_num, packet->cin, packet->num_midi_bytes);
}

static void packet_from_midi_bytes(uint8_t *midi_bytes, uint8_t num_midi_bytes, uint8_t cable_num, struct usb_midi_packet *packet)
{
	packet->cable_num = cable_num;
	packet->num_midi_bytes = num_midi_bytes;

	uint8_t first_byte = midi_bytes[0];

	// TODO: actually compute CIN
	enum usb_midi_cin cin = (first_byte & 0xf0) >> 4;
	packet->cin = cin;

	// Put cable number and CIN in packet byte 0
	packet->bytes[0] = (cable_num << 4) | cin;

	// Fill packet bytes 1,2 and 3 with zero padded midi bytes.
	for (int i = 0; i < 3; i++)
	{
		uint8_t midi_byte = i < num_midi_bytes ? midi_bytes[i] : 0;
		packet->bytes[i + 1] = midi_byte;
	}
}

static void packet_from_usb_bytes(uint8_t packet_bytes[4], struct usb_midi_packet *packet)
{
	// LOG_DBG("Parsing packet bytes %02x %02x %02x %02x", packet_bytes[0], packet_bytes[1], packet_bytes[2], packet_bytes[3]);

	packet->num_midi_bytes = 0;

	packet->bytes[0] = packet_bytes[0];
	packet->bytes[1] = packet_bytes[1];
	packet->bytes[2] = packet_bytes[2];
	packet->bytes[3] = packet_bytes[3];

	packet->cable_num = (packet_bytes[0] & 0xf0) >> 4;
	packet->cin = packet_bytes[0] & 0xf;

	switch (packet->cin)
	{
	case USB_MIDI_CIN_MISC:
	case USB_MIDI_CIN_CABLE_EVENT:
		// Reserved for future expansion. Set the byte count at 0 to
		// indicate that this packet should be ignored.
		// LOG_WRN("Got reserved code index number %d", packet->cin);
		packet->num_midi_bytes = 0;
		break;
	case USB_MIDI_CIN_SYSEX_END_1BYTE:
	case USB_MIDI_CIN_1BYTE_DATA:
		packet->num_midi_bytes = 1;
		break;
	case USB_MIDI_CIN_SYSCOM_2BYTE:
	case USB_MIDI_CIN_SYSEX_END_2BYTE:
	case USB_MIDI_CIN_PROGRAM_CHANGE:
	case USB_MIDI_CIN_CHANNEL_PRESSURE:
		packet->num_midi_bytes = 2;
		break;
	default:
		packet->num_midi_bytes = 3;
		break;
	}

	// LOG_DBG("Parsed packet cin %d, num_midi_bytes %d", packet->cin, packet->num_midi_bytes);
}

void usb_midi_register_handlers(struct usb_midi_handlers *h)
{
	handlers.enabled_cb = h->enabled_cb;
	handlers.rx_cb = h->rx_cb;
}

uint32_t usb_midi_tx(uint8_t cable_number, uint8_t *midi_bytes, uint8_t midi_byte_count)
{
	struct usb_midi_packet packet;
	packet_from_midi_bytes(midi_bytes, midi_byte_count, cable_number, &packet);
	// printk("tx ");
	// log_packet(&packet);
	uint32_t num_written_bytes = 0;
	usb_write(0x81, packet.bytes, 4, &num_written_bytes);
	return num_written_bytes;
}

static void midi_out_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code
					   ep_status)
{
	uint8_t buf[4];
	uint32_t num_read_bytes = 0;
	usb_read(ep, buf, 4, &num_read_bytes);
	struct usb_midi_packet packet;
	packet_from_usb_bytes(buf, &packet);
	if (handlers.rx_cb && packet.num_midi_bytes)
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
    },
};

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
		if (!usb_midi_is_enabled && handlers.enabled_cb)
		{
			handlers.enabled_cb(true);
		}
		usb_midi_is_enabled = true;
		// printk("USB_DC_CONFIGURED\n");
		break;
	/** USB connection lost */
	case USB_DC_DISCONNECTED:
		// printk("USB_DC_DISCONNECTED\n");
		break;
	/** USB connection suspended by the HOST */
	case USB_DC_SUSPEND:
		if (usb_midi_is_enabled && handlers.enabled_cb)
		{
			handlers.enabled_cb(false);
		}
		usb_midi_is_enabled = false;
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