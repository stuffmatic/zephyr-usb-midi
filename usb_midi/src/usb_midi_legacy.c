#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/ring_buffer.h>
#include <usb_descriptor.h>
#include <usb_midi/usb_midi.h>
#include "usb_midi_types.h"
#include "usb_midi_macros.h"
#include "usb_midi_packet.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);

#define LOG_DBG_PACKET(packet) LOG_DBG("%02x %02x %02x %02x | cable %02x | CIN %01x | %d MIDI bytes", \
									   packet.bytes[0], packet.bytes[1], packet.bytes[2], packet.bytes[3],            \
									   packet.cable_num, packet.cin, packet.num_midi_bytes)

#ifdef CONFIG_USB_DEVICE_SOF
static uint8_t tx_fifo_data[CONFIG_USB_MIDI_TX_FIFO_SIZE];
static struct ring_buf tx_fifo = {
	.buffer = tx_fifo_data, 
	.size = CONFIG_USB_MIDI_TX_FIFO_SIZE
};
static uint8_t tx_temp_buf[USB_MIDI_EP_MAX_PACKET_SIZE];
static int has_pending_buffer = 0;
#endif

#ifdef CONFIG_USB_MIDI_CUSTOM_JACK_NAMES

/* Macro for defining an out jack string descriptor */
#define OUTPUT_JACK_STRING_DESCR(jack_number, _)                                               \
    USBD_STRING_DESCR_USER_DEFINE(primary) struct output_jack_##jack_number##_string_descr_type                                       \
    {                                                                                          \
        uint8_t bLength;                                                                       \
        uint8_t bDescriptorType;                                                               \
        uint8_t bString[USB_BSTRING_LENGTH(CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME)]; \
    } __packed output_jack_##jack_number##_string_descr = { \
		.bLength =	USB_STRING_DESCRIPTOR_LENGTH(CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME),			\
		.bDescriptorType = USB_DESC_STRING, \
		.bString = CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME \
	};

/* Macro for defining an in jack string descriptor */
#define INPUT_JACK_STRING_DESCR(jack_number, _)                                               \
    USBD_STRING_DESCR_USER_DEFINE(primary) struct input_jack_##jack_number##_string_descr_type                                       \
    {                                                                                         \
        uint8_t bLength;                                                                      \
        uint8_t bDescriptorType;                                                              \
        uint8_t bString[USB_BSTRING_LENGTH(CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME)]; \
    } __packed input_jack_##jack_number##_string_descr = { \
		.bLength =	USB_STRING_DESCRIPTOR_LENGTH(CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME),			\
		.bDescriptorType = USB_DESC_STRING, \
		.bString = CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME \
	};

/* Define in/out jack string descriptors */
LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, OUTPUT_JACK_STRING_DESCR, ())
LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, INPUT_JACK_STRING_DESCR, ())

// Macros for getting pointers to in/out jack string descriptors
#define INPUT_JACK_DESC_PTR(jack_number, _) &input_jack_##jack_number##_string_descr
#define OUTPUT_JACK_DESC_PTR(jack_number, _) &output_jack_##jack_number##_string_descr

// Define run time addressable arrays of pointers to in/out jack string descriptors
struct usb_desc_header* input_jack_descs[] = {
    LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, INPUT_JACK_DESC_PTR, (,))
};
struct usb_desc_header* output_jack_descs[] = {
    LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, OUTPUT_JACK_DESC_PTR, (,))
};

#endif /* CONFIG_USB_MIDI_CUSTOM_JACK_NAMES */


USBD_CLASS_DESCR_DEFINE(primary, 0)
struct usb_midi_config usb_midi_config_data = {
	.ac_if = INIT_AC_IF,
	.ac_cs_if = INIT_AC_CS_IF,
	.ms_if = INIT_MS_IF,
	.ms_cs_if = INIT_MS_CS_IF,
	.out_jacks_emb = {
		LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, INIT_OUT_JACK, (, ), 0)},
	.in_jacks_emb = {LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, INIT_IN_JACK, (, ), CONFIG_USB_MIDI_NUM_OUTPUTS)},
	.element = INIT_ELEMENT,
	.in_ep = INIT_IN_EP,
	.in_cs_ep = {.bLength = sizeof(struct usb_midi_bulk_in_ep_descriptor), .bDescriptorType = USB_DESC_CS_ENDPOINT, .bDescriptorSubtype = 0x01, .bNumEmbMIDIJack = CONFIG_USB_MIDI_NUM_OUTPUTS, .BaAssocJackID = {LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, IDX_WITH_OFFSET, (, ), 1)}},
	.out_ep = INIT_OUT_EP,
	.out_cs_ep = {.bLength = sizeof(struct usb_midi_bulk_out_ep_descriptor), .bDescriptorType = USB_DESC_CS_ENDPOINT, .bDescriptorSubtype = 0x01, .bNumEmbMIDIJack = CONFIG_USB_MIDI_NUM_INPUTS, .BaAssocJackID = {LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, IDX_WITH_OFFSET, (, ), 1 + CONFIG_USB_MIDI_NUM_OUTPUTS)}}};

static int usb_midi_is_available = false;
static struct usb_midi_cb_t user_callbacks = {
	.available_cb = NULL,
	.midi_message_cb = NULL,
	.tx_done_cb = NULL,
	.sysex_data_cb = NULL,
	.sysex_end_cb = NULL,
	.sysex_start_cb = NULL};

static void availability_changed(int is_available) {
	if (usb_midi_is_available == is_available) {
		return;
	}

	LOG_INF("device became %s ", is_available ? "available" : "unavailable");
#ifdef CONFIG_USB_DEVICE_SOF
	if (is_available) {
		ring_buf_reset(&tx_fifo);
	}
#endif
	if (user_callbacks.available_cb) {
		user_callbacks.available_cb(is_available);
	}
	usb_midi_is_available = is_available;
}

void usb_midi_register_callbacks(struct usb_midi_cb_t *cb)
{
	user_callbacks.available_cb = cb->available_cb;
	user_callbacks.midi_message_cb = cb->midi_message_cb;
	user_callbacks.tx_done_cb = cb->tx_done_cb;
	user_callbacks.sysex_start_cb = cb->sysex_start_cb;
	user_callbacks.sysex_data_cb = cb->sysex_data_cb;
	user_callbacks.sysex_end_cb = cb->sysex_end_cb;
}

static void midi_out_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	if (ep_status == USB_DC_EP_DATA_OUT) {
		uint8_t buf[4];
		uint32_t num_read_bytes = 1;
		while (num_read_bytes > 0)
		{
			int read_rc = usb_read(ep, buf, 4, &num_read_bytes);
			if (num_read_bytes == 0)
			{
				break;
			}
			struct usb_midi_packet_t packet;
			enum usb_midi_packet_error_t error = usb_midi_packet_from_usb_bytes(buf, &packet);

			if (error != USB_MIDI_PACKET_SUCCESS)
			{
				LOG_ERR("Failed to read packet with error %d", error);
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
				if (error != USB_MIDI_PACKET_SUCCESS)
				{
					LOG_ERR("Failed to parse packet with error %d", error);
				}
			}
		}
	} else {
		// printk("USB ep status %d\n", ep_status);
	}
}

// return non-zero if a new buffer was enqueued, zero otherwise
static int enqueue_next_tx_buf()
{
	if (ring_buf_is_empty(&tx_fifo)) {
		return 0;
	}
	
	// Read fifo data into the tx temp buffer. Don't read
	// more than we can fit into the buffer.
	int num_bytes_in_fifo = ring_buf_size_get(&tx_fifo);
	__ASSERT_NO_MSG(num_bytes_in_fifo % 4 == 0);
	__ASSERT_NO_MSG(buf->size % 4 == 0);
	int num_bytes_to_add = num_bytes_in_fifo > USB_MIDI_EP_MAX_PACKET_SIZE ? USB_MIDI_EP_MAX_PACKET_SIZE : num_bytes_in_fifo;
	int num_bytes_added = ring_buf_get(&tx_fifo, tx_temp_buf, num_bytes_to_add);
	int write_result = usb_write(USB_MIDI_IN_EP_ADDR, tx_temp_buf, num_bytes_added, NULL);
	
	return 0;
}

static void midi_in_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	int invoke_tx_done = ep_status == USB_DC_EP_DATA_IN && user_callbacks.tx_done_cb;
#ifdef CONFIG_USB_DEVICE_SOF
	has_pending_buffer = enqueue_next_tx_buf();
	if (ring_buf_space_get(&tx_fifo) > CONFIG_USB_MIDI_TX_FIFO_WATER_MARK)
	{
		invoke_tx_done = 0;
	}
#endif
	user_callbacks.tx_done_cb();
}

static struct usb_ep_cfg_data midi_ep_cfg[] = {
	{
		.ep_cb = midi_in_ep_cb,
		.ep_addr = USB_MIDI_IN_EP_ADDR,
	},
	{
		.ep_cb = midi_out_ep_cb,
		.ep_addr = USB_MIDI_OUT_EP_ADDR,
	}};

static void usb_status_callback(struct usb_cfg_data *cfg,
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
		has_pending_buffer = 0;
		availability_changed(1);
		break;
	/** USB connection lost */
	case USB_DC_DISCONNECTED:
		LOG_DBG("USB_DC_DISCONNECTED");
		break;
	/** USB connection suspended by the HOST */
	case USB_DC_SUSPEND:
		availability_changed(0);
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
		if (!has_pending_buffer) {
			has_pending_buffer = enqueue_next_tx_buf();
		}
		// LOG_DBG("USB_DC_SOF");
		break;
	/** Initial USB connection status */
	case USB_DC_UNKNOWN:
		LOG_DBG("USB_DC_UNKNOWN");
		break;
	}
}

static void usb_interface_config_cb(struct usb_desc_header *head, uint8_t bInterfaceNumber) 
{
	#ifdef CONFIG_USB_MIDI_CUSTOM_JACK_NAMES
	for (int i = 0; i < CONFIG_USB_MIDI_NUM_INPUTS; i++) {
		int descr = input_jack_descs[i];
		int descr_idx = usb_get_str_descriptor_idx(descr);
		usb_midi_config_data.in_jacks_emb[i].iJack = descr_idx;
		LOG_DBG("Assigned string descriptor %d to input jack %d", descr_idx, i);
	}

	for (int i = 0; i < CONFIG_USB_MIDI_NUM_OUTPUTS; i++) {
		int descr = output_jack_descs[i];
		int descr_idx = usb_get_str_descriptor_idx(descr);
		usb_midi_config_data.out_jacks_emb[i].iJack = descr_idx;
		LOG_DBG("Assigned string descriptor %d to output jack %d", descr_idx, i);
	}
	
	#endif
}

enum usb_midi_error_t usb_midi_tx(uint8_t cable_number, uint8_t *midi_bytes)
{
	if (!usb_midi_is_available) {
		return USB_MIDI_NOT_AVAILABLE;
	}

	struct usb_midi_packet_t packet;
	enum usb_midi_packet_error_t error = usb_midi_packet_from_midi_bytes(midi_bytes, cable_number, &packet);
	if (error != USB_MIDI_PACKET_SUCCESS)
	{
		LOG_ERR("Building packet from MIDI bytes %02x %02x %02x failed with error %d", midi_bytes[0], midi_bytes[1], midi_bytes[2], error);
		return USB_MIDI_INVALID_DATA;
	}
	LOG_DBG_PACKET(packet);
#ifdef CONFIG_USB_DEVICE_SOF
	if (ring_buf_space_get(&tx_fifo) < 4) {
		return USB_MIDI_TX_FIFO_FULL;
	}
	int put_result = ring_buf_put(&tx_fifo, packet.bytes, 4);
	__ASSERT(put_result == 0, "USB MIDI packet should fit in tx FIFO");
#else
	int write_result = usb_write(USB_MIDI_IN_EP_ADDR, packet.bytes, 4, NULL);
	// assume usb_write error is
	return write_result == 0 ? USB_MIDI_SUCCESS : USB_MIDI_TX_FAILED;
#endif
}

USBD_DEFINE_CFG_DATA(usb_midi_config) = {
	.usb_device_description = NULL,
	.interface_config = usb_interface_config_cb,
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