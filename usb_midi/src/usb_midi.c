#include <zephyr/init.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/usb/udc.h>
#include <usb_midi/usb_midi.h>
#include "usb_midi_types.h"
#include "usb_midi_macros.h"
#include "usb_midi_packet.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usb_midi, CONFIG_USB_MIDI_LOG_LEVEL);

#define LOG_DBG_PACKET(packet)                                                                     \
	LOG_DBG("%02x %02x %02x %02x | cable %02x | CIN %01x | %d MIDI bytes", packet.bytes[0],    \
		packet.bytes[1], packet.bytes[2], packet.bytes[3], packet.cable_num, packet.cin,   \
		packet.num_midi_bytes)

static struct usbd_class_data usb_midi; // TODO: remove


#ifdef CONFIG_USB_MIDI_CUSTOM_JACK_NAMES

// Macros for defining in/out jack string descriptor nodes for given jack numbers
#define INIT_INPUT_JACK_STRING_DESCR(jack_number, _) USBD_DESC_STRING_DEFINE(in_jack_##jack_number##_string_desc, CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME, USBD_DUT_STRING_INTERFACE)
#define INIT_OUTPUT_JACK_STRING_DESCR(jack_number, _) USBD_DESC_STRING_DEFINE(out_jack_##jack_number##_string_desc, CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME, USBD_DUT_STRING_INTERFACE)

// Define in/out jack string descriptors. Named out_jack_[idx]_string_desc and in_jack_[idx]_string_desc
LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, INIT_OUTPUT_JACK_STRING_DESCR, (;));
LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, INIT_INPUT_JACK_STRING_DESCR, (;));

// Macros for getting pointers to in/out jack string descriptors
#define OUT_JACK_DESCR_PTR(i, _) (struct usbd_desc_node *)&out_jack_##i##_string_desc
#define IN_JACK_DESCR_PTR(i, _) (struct usbd_desc_node *)&in_jack_##i##_string_desc

// Define run time addressable arrays of pointers to in/out jack string descriptors
static struct usbd_desc_node* in_jack_descs[CONFIG_USB_MIDI_NUM_INPUTS] = {
	LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, IN_JACK_DESCR_PTR, (, ))
};
static struct usbd_desc_node* out_jack_descs[CONFIG_USB_MIDI_NUM_OUTPUTS] = {
	LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, OUT_JACK_DESCR_PTR, (, ))
};
#endif /* CONFIG_USB_MIDI_CUSTOM_JACK_NAMES */


struct usb_midi_config usb_midi_config_data = {
	.ac_if = INIT_AC_IF,
	.ac_cs_if = INIT_AC_CS_IF,
	.ms_if = INIT_MS_IF,
	.ms_cs_if = INIT_MS_CS_IF,
	.out_jacks_emb = {LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, INIT_OUT_JACK, (, ), 0)},
	.in_jacks_emb = {LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, INIT_IN_JACK, (, ),
				 CONFIG_USB_MIDI_NUM_OUTPUTS)},
	.element = INIT_ELEMENT,
	.in_ep = INIT_IN_EP,
	.in_cs_ep = {.bLength = sizeof(struct usb_midi_bulk_in_ep_descriptor),
		     .bDescriptorType = USB_DESC_CS_ENDPOINT,
		     .bDescriptorSubtype = 0x01,
		     .bNumEmbMIDIJack = CONFIG_USB_MIDI_NUM_OUTPUTS,
		     .BaAssocJackID = {LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, IDX_WITH_OFFSET, (, ),
					       1)}},
	.out_ep = INIT_OUT_EP,
	.out_cs_ep = {.bLength = sizeof(struct usb_midi_bulk_out_ep_descriptor),
		      .bDescriptorType = USB_DESC_CS_ENDPOINT,
		      .bDescriptorSubtype = 0x01,
		      .bNumEmbMIDIJack = CONFIG_USB_MIDI_NUM_INPUTS,
		      .BaAssocJackID = {LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, IDX_WITH_OFFSET, (, ),
						1 + CONFIG_USB_MIDI_NUM_OUTPUTS)}}};

static struct usb_desc_header nil_desc = {
	.bLength = 0,
	.bDescriptorType = 0,
};

#define OUT_JACK_PTR(i, _) (struct usb_desc_header *)&usb_midi_config_data.out_jacks_emb[i]
#define IN_JACK_PTR(i, _)  (struct usb_desc_header *)&usb_midi_config_data.in_jacks_emb[i]

const static struct usb_desc_header *xxx[] = {
	(struct usb_desc_header *)&usb_midi_config_data.ac_if,
	(struct usb_desc_header *)&usb_midi_config_data.ac_cs_if,
	(struct usb_desc_header *)&usb_midi_config_data.ms_if,
	(struct usb_desc_header *)&usb_midi_config_data.ms_cs_if,
#if CONFIG_USB_MIDI_NUM_OUTPUTS > 0
	LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, OUT_JACK_PTR, (, )),
#endif
#if CONFIG_USB_MIDI_NUM_INPUTS > 0
	LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, IN_JACK_PTR, (, )),
#endif
	(struct usb_desc_header *)&usb_midi_config_data.element,
	(struct usb_desc_header *)&usb_midi_config_data.in_ep,
	(struct usb_desc_header *)&usb_midi_config_data.in_cs_ep,
	(struct usb_desc_header *)&usb_midi_config_data.out_ep,
	(struct usb_desc_header *)&usb_midi_config_data.out_cs_ep,
	&nil_desc};

struct usb_midi_data {
	// fifo used to enqueue 4 byte USB MIDI packets to send at the next SOF event
	struct ring_buf tx_fifo;
	int has_pending_tx_buffer;
	int is_available;
	const struct usb_desc_header **const fs_desc;
	const struct usb_desc_header **const hs_desc;
};

static uint8_t tx_fifo_data[CONFIG_USB_MIDI_TX_FIFO_SIZE];
static struct usb_midi_data usb_midi_class_data = {
	.tx_fifo = {.buffer = tx_fifo_data, .size = CONFIG_USB_MIDI_TX_FIFO_SIZE},
	// .rx_buf = NULL,
	.has_pending_tx_buffer = 0,
	.is_available = 0,
	.fs_desc = &xxx[0],
	.hs_desc = &xxx[0],
};

static int temp_tx_buffer_size = 0;
static uint8_t temp_tx_buffer[USB_MIDI_EP_MAX_PACKET_SIZE];

static int usb_midi_is_available = false;
static struct usb_midi_cb_t user_callbacks = {.available_cb = NULL,
					      .midi_message_cb = NULL,
					      .tx_done_cb = NULL,
					      .sysex_data_cb = NULL,
					      .sysex_end_cb = NULL,
					      .sysex_start_cb = NULL};

enum usb_midi_error_t usb_midi_tx(uint8_t cable_number, uint8_t *midi_bytes)
{
	struct usb_midi_data *data = &usb_midi_class_data;

	if (!data->is_available) {
		return USB_MIDI_NOT_AVAILABLE;
	}

	if (ring_buf_space_get(&data->tx_fifo) < 4) {
		LOG_WRN("tx fifo is full");
		return USB_MIDI_TX_FIFO_FULL;
	}

	struct usb_midi_packet_t packet;
	enum usb_midi_packet_error_t error =
		usb_midi_packet_from_midi_bytes(midi_bytes, cable_number, &packet);
	if (error != USB_MIDI_PACKET_SUCCESS) {
		LOG_ERR("Building tx packet from MIDI bytes %02x %02x %02x failed with error %d",
			midi_bytes[0], midi_bytes[1], midi_bytes[2], error);
		return USB_MIDI_INVALID_DATA;
	}
	LOG_DBG_PACKET(packet);
	
	int put_result = ring_buf_put(&data->tx_fifo, packet.bytes, 4);
	__ASSERT(put_result == 4, "USB MIDI packet should fit in tx FIFO");
	
	return USB_MIDI_SUCCESS;
}

// debug counter
int debug_tx_ep_buf_balance = 0;

// return non-zero if a new buffer was enqueued, zero otherwise
static int enqueue_next_tx_buf(struct usbd_class_data *const c_data)
{
	struct usb_midi_data *data = usbd_class_get_private(c_data);
	if (ring_buf_is_empty(&data->tx_fifo)) {
		return 0;
	}
	
	// Allocate a new endpoint buffer to enqueue
	struct net_buf *buf = usbd_ep_buf_alloc(c_data, USB_MIDI_IN_EP_ADDR, USB_MIDI_EP_MAX_PACKET_SIZE);
	if (buf == NULL) {
		LOG_ERR("Failed to allocate tx ep buf, balance %d", debug_tx_ep_buf_balance);
		return 0;
	}
	debug_tx_ep_buf_balance++;

	// Read fifo data into the endpoint buffer. Don't read
	// more than we can fit into the buffer.
	int num_bytes_in_fifo = ring_buf_size_get(&data->tx_fifo);
	__ASSERT_NO_MSG(num_bytes_in_fifo % 4 == 0);
	__ASSERT_NO_MSG(buf->size % 4 == 0);
	int num_bytes_to_add = num_bytes_in_fifo > buf->size ? buf->size : num_bytes_in_fifo;
	int num_bytes_read = ring_buf_get(&data->tx_fifo, buf->data, num_bytes_to_add);
	buf->len = num_bytes_read;
	if (num_bytes_read != num_bytes_to_add) {
		LOG_ERR("Expected to read %d bytes from tx fifo, read %d", num_bytes_to_add, num_bytes_read);
	}

	int enqueue_result = usbd_ep_enqueue(c_data, buf);
	if (enqueue_result != 0) {
		// something else went wrong. free tx buffer. this shouldn't happen.
		LOG_ERR("usbd_ep_enqueue failed with error %d", enqueue_result);
		usbd_ep_buf_free(c_data->uds_ctx, buf);
	} else {
		return 1;
	}
	
	return 0;
}

/** Feature halt state update handler */
void usb_midi_feature_halt_cb(struct usbd_class_data *const c_data, uint8_t ep, bool halted)
{
	LOG_DBG("Instance %p, ep %u, halted %d", c_data, ep, halted);
}

/** Configuration update handler */
void usb_midi_update_cb(struct usbd_class_data *const c_data, uint8_t iface, uint8_t alternate)
{
	LOG_DBG("Instance %p, interface %u alternate %u changed", c_data, iface, alternate);
}

/** Endpoint request completion event handler */
int usb_midi_request_cb(struct usbd_class_data *const c_data, struct net_buf *buf, int err)
{
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct udc_buf_info *bi = NULL;
	bi = (struct udc_buf_info *)net_buf_user_data(buf);
	LOG_DBG("%p -> ep 0x%02x, len %u, err %d", c_data, bi->ep, buf->len, err);

	// TODO: check error/status before doing this?

	if (USB_EP_DIR_IS_OUT(bi->ep)) {
		// Received data.
		struct usb_midi_packet_t packet;
		struct usb_midi_parse_cb_t parse_cb = {
			.message_cb = user_callbacks.midi_message_cb,
			.sysex_data_cb = user_callbacks.sysex_data_cb,
			.sysex_end_cb = user_callbacks.sysex_end_cb,
			.sysex_start_cb = user_callbacks.sysex_start_cb
		};
		if (buf->len % 4 != 0) {
			LOG_WRN("expected rx buffer length to be a multiple of 4, got %d", buf->len);
		}
		int read_pos = 0;
		// Assume the input buffer length is a multiple 
		// of 4 and ignore any additional bytes.
		while (read_pos < buf->len) {
			int bytes_left = buf->len - read_pos;
			if (bytes_left < 4) {
				break;
			}
			int decode_result =
				usb_midi_packet_from_usb_bytes(&buf->data[read_pos], &packet);
			if (decode_result == USB_MIDI_PACKET_SUCCESS) {
				usb_midi_parse_packet(packet.bytes, &parse_cb);
			} else {
				LOG_WRN("decoding USB MIDI rx packet failed with error %d",
					decode_result);
			}
			LOG_DBG_PACKET(packet);
			read_pos += 4;
		}

		// free the current buffer...
		usbd_ep_buf_free(uds_ctx, buf);

		// ...and allocate and enqueue new one for receiving future data
		struct net_buf *next_buf = usbd_ep_buf_alloc(c_data, USB_MIDI_OUT_EP_ADDR, USB_MIDI_EP_MAX_PACKET_SIZE);
		int r = usbd_ep_enqueue(c_data, next_buf);

	} else {
		struct usb_midi_data *data = usbd_class_get_private(c_data);

		// sent data to host. free the buffer...
		usbd_ep_buf_free(uds_ctx, buf);
		debug_tx_ep_buf_balance--;
		// ...and enqueue next tx endpoint buffer if there is data in the tx FIFO
		data->has_pending_tx_buffer = enqueue_next_tx_buf(c_data);

		// If there is enough room in the tx fifo, signal that more data may be added.
		if (user_callbacks.tx_done_cb && ring_buf_space_get(&data->tx_fifo) <= CONFIG_USB_MIDI_TX_FIFO_WATER_MARK) {
			user_callbacks.tx_done_cb();
		}
	}

	return 0;
}

/** USB power management handler suspended */
void usb_midi_suspended_cb(struct usbd_class_data *const c_data)
{
	LOG_INF("Instance %p", c_data);
}

/** USB power management handler resumed */
void usb_midi_resumed_cb(struct usbd_class_data *const c_data)
{
	LOG_INF("Instance %p", c_data);
}

/** Start of Frame */
void usb_midi_sof_cb(struct usbd_class_data *const c_data)
{
	struct usb_midi_data *data = usbd_class_get_private(c_data);
	if (!data->has_pending_tx_buffer) {
		data->has_pending_tx_buffer = enqueue_next_tx_buf(c_data);
	}
}

/** Class associated configuration is selected */
void usb_midi_enable_cb(struct usbd_class_data *const c_data)
{
	struct usb_midi_data *data = usbd_class_get_private(c_data);
	ring_buf_reset(&data->tx_fifo);
	data->is_available = 1;

	LOG_DBG("Instance %p", c_data);
	if (user_callbacks.available_cb) {
		LOG_INF("USB MIDI became available");
		user_callbacks.available_cb(1);
	}
	
	// Allocate buffer for receiving data
	struct net_buf *rx_buf = usbd_ep_buf_alloc(c_data, USB_MIDI_OUT_EP_ADDR, USB_MIDI_EP_MAX_PACKET_SIZE);
	// Enqueue the rx buffer. This signals to the stack that
	// we're ready to receive data. If this is not done,
	// nothing will be received.
	int enqueue_result = usbd_ep_enqueue(c_data, rx_buf);
	if (enqueue_result != 0) {
		LOG_ERR("Failed to enqueue rx buf with error %d", enqueue_result);
	}
}

/** Class associated configuration is disabled */
void usb_midi_disable_cb(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
	struct usb_midi_data *data = usbd_class_get_private(c_data);

	data->is_available = 0;
	if (user_callbacks.available_cb) {
		LOG_INF("USB MIDI became unavailable");
		user_callbacks.available_cb(0);
	}
}

/** Initialization of the class implementation */
int usb_midi_init_cb(struct usbd_class_data *const c_data)
{
	LOG_INF("Instance %p", c_data);

#ifdef CONFIG_USB_MIDI_CUSTOM_JACK_NAMES
	for (int i = 0; i < CONFIG_USB_MIDI_NUM_INPUTS; i++) {
		struct usb_desc_header* hdr = in_jack_descs[i];
		int add_result = usbd_add_descriptor(c_data->uds_ctx, hdr);
		if (add_result == 0) {
			uint8_t idx = usbd_str_desc_get_idx(hdr);
			usb_midi_config_data.in_jacks_emb[i].iJack = idx;
			LOG_DBG("Assigned string descriptor %d to input jack %d", idx, i);
		} else {
			LOG_ERR("Failed to add input jack %d string descriptor with error", i, add_result);
		}
	}
	for (int i = 0; i < CONFIG_USB_MIDI_NUM_OUTPUTS; i++) {
		struct usb_desc_header* hdr = out_jack_descs[i];
		int add_result = usbd_add_descriptor(c_data->uds_ctx, hdr);
		if (add_result == 0) {
			uint8_t idx = usbd_str_desc_get_idx(hdr);
			usb_midi_config_data.out_jacks_emb[i].iJack = idx;
			LOG_DBG("Assigned string descriptor %d to output jack %d", idx, i);
		} else {
			LOG_ERR("Failed to add output jack %d string descriptor with error", i, add_result);
		}
	}
#endif
	return 0;
}

/** Shutdown of the class implementation */
void usb_midi_shutdown_cb(struct usbd_class_data *const c_data)
{
	LOG_INF("Instance %p", c_data);

#ifdef CONFIG_USB_MIDI_CUSTOM_JACK_NAMES
for (int i = 0; i < CONFIG_USB_MIDI_NUM_INPUTS; i++) {
		struct usbd_desc_node* desc = in_jack_descs[i];
		usbd_remove_descriptor(desc);
		LOG_DBG("Removed string descriptor from input jack %d", i);
	}
	for (int i = 0; i < CONFIG_USB_MIDI_NUM_OUTPUTS; i++) {
		struct usb_desc_header* desc = out_jack_descs[i];
		usbd_remove_descriptor(desc);
		LOG_DBG("Removed string descriptor from output jack %d", i);
	}
#endif
}

/** Get function descriptor based on speed parameter */
void *usb_midi_get_desc_cb(struct usbd_class_data *const c_data, const enum usbd_speed speed)
{
	LOG_DBG("Instance %p, speed %d", c_data, speed);

	struct usb_midi_data *data = usbd_class_get_private(c_data);

	if (speed == USBD_SPEED_HS) {
		return data->hs_desc;
	}

	return data->fs_desc;
}

struct usbd_class_api usb_midi_class_api = {.feature_halt = usb_midi_feature_halt_cb,
					    .update = usb_midi_update_cb,
					    .request = usb_midi_request_cb,
					    .suspended = usb_midi_suspended_cb,
					    .resumed = usb_midi_resumed_cb,
					    .sof = usb_midi_sof_cb,
					    .enable = usb_midi_enable_cb,
					    .disable = usb_midi_disable_cb,
					    .init = usb_midi_init_cb,
					    .shutdown = usb_midi_shutdown_cb,
					    .get_desc = usb_midi_get_desc_cb};

USBD_DEFINE_CLASS(usb_midi, &usb_midi_class_api, &usb_midi_class_data, NULL);

void usb_midi_register_callbacks(struct usb_midi_cb_t *cb)
{
	user_callbacks.available_cb = cb->available_cb;
	user_callbacks.midi_message_cb = cb->midi_message_cb;
	user_callbacks.tx_done_cb = cb->tx_done_cb;
	user_callbacks.sysex_start_cb = cb->sysex_start_cb;
	user_callbacks.sysex_data_cb = cb->sysex_data_cb;
	user_callbacks.sysex_end_cb = cb->sysex_end_cb;
}