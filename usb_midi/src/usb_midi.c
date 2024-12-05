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
						1 + CONFIG_USB_MIDI_NUM_OUTPUTS)}}
};

static struct usb_desc_header nil_desc = {								\
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
	&nil_desc
};

struct usb_midi_data {
	// re-usable buffer used for receiving data
	struct net_buf* rx_buf;
	// re-usable buffer used for sending data
	struct net_buf* tx_buf;
	// fifo used to enqueue packets to send at the next SOF event
	struct ring_buf tx_fifo;
	const struct usb_desc_header **const fs_desc;
	const struct usb_desc_header **const hs_desc;
};

static uint8_t tx_fifo_data[CONFIG_USB_MIDI_TX_FIFO_SIZE];
static struct usb_midi_data usb_midi_class_data = {
	.tx_fifo = {
		.buffer = tx_fifo_data,
		.size = CONFIG_USB_MIDI_TX_FIFO_SIZE
	},
	.rx_buf = NULL,
	.fs_desc = &xxx[0],
	.hs_desc = &xxx[0],
};

static int temp_tx_buffer_size = 0;
static uint8_t temp_tx_buffer[EP_MAX_PACKET_SIZE];

static int usb_midi_is_available = false;
static struct usb_midi_cb_t user_callbacks = {.available_cb = NULL,
					      .midi_message_cb = NULL,
					      .tx_done_cb = NULL,
					      .sysex_data_cb = NULL,
					      .sysex_end_cb = NULL,
					      .sysex_start_cb = NULL};

int usb_midi_tx(uint8_t cable_number, uint8_t *midi_bytes)
{
	struct usb_midi_packet_t packet;
	enum usb_midi_error_t error = usb_midi_packet_from_midi_bytes(midi_bytes, cable_number, &packet); 
	if (error != USB_MIDI_SUCCESS)
	{
		LOG_ERR("Building packet from MIDI bytes %02x %02x %02x failed with error %d",
		midi_bytes[0], midi_bytes[1], midi_bytes[2], error); return -EINVAL;
		return error;
	}
	LOG_DBG_PACKET(packet);
	struct usb_midi_data *data = &usb_midi_class_data;
	int put_result = ring_buf_put(&data->tx_fifo, packet.bytes, 4);
	if (put_result != 4) {
		LOG_WRN("Failed to add packet to tx fifo");
		return -1; // TODO: proper error
	}
	return 0;
}

int usb_midi_tx_buffer_is_full()
{
	return temp_tx_buffer_size == EP_MAX_PACKET_SIZE;
}

int usb_midi_tx_buffer_add(uint8_t cable_number, uint8_t *midi_bytes)
{
	if (usb_midi_tx_buffer_is_full()) {
		return -1;
	}

	struct usb_midi_packet_t packet;
	enum usb_midi_error_t error =
		usb_midi_packet_from_midi_bytes(midi_bytes, cable_number, &packet);
	if (error != USB_MIDI_SUCCESS) {
		LOG_ERR("Building packet from MIDI bytes %02x %02x %02x failed with error %d",
			midi_bytes[0], midi_bytes[1], midi_bytes[2], error);
		return -EINVAL;
	}

	for (int i = 0; i < 4; i++) {
		temp_tx_buffer[temp_tx_buffer_size] = packet.bytes[i];
		temp_tx_buffer_size++;
	}
	return 0;
}

int usb_midi_tx_buffer_send()
{
	/* if (temp_tx_buffer_size > 0) {
		int write_result = usb_write(0x81, temp_tx_buffer, temp_tx_buffer_size, NULL);
		if (write_result == 0) {
			temp_tx_buffer_size = 0;
		}
		return write_result;
	}
	return 0; */
}

/** Feature halt state update handler */
void usb_midi_feature_halt_cb(struct usbd_class_data *const c_data, uint8_t ep, bool halted)
{
	LOG_DBG("Instance %p, ep %u, halted %d",
		c_data, ep, halted);
}

/** Configuration update handler */
void usb_midi_update_cb(struct usbd_class_data *const c_data, uint8_t iface, uint8_t alternate)
{
	LOG_DBG("Instance %p, interface %u alternate %u changed",
		c_data, iface, alternate);
}

/** Endpoint request completion event handler */
int usb_midi_request_cb(struct usbd_class_data *const c_data, struct net_buf *buf, int err)
{
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct udc_buf_info *bi = NULL;
	bi = (struct udc_buf_info *)net_buf_user_data(buf);
	LOG_DBG("%p -> ep 0x%02x, len %u, err %d", c_data, bi->ep, buf->len, err);

	// TODO: check error before doing this?
	// TODO: don't hardcode endpoint addresses
	if (bi->ep == 0x01) {
		// received data. TODO: handle more than 4 bytes
		struct usb_midi_packet_t packet;
		__ASSERT(buf->size % 4 == 0, "ep buf should only contain 4 byte packets");
		int decode_result = usb_midi_packet_from_usb_bytes(buf->data, &packet);
		if (decode_result == USB_MIDI_SUCCESS) {
			struct usb_midi_parse_cb_t parse_cb = {
					.message_cb = user_callbacks.midi_message_cb,
					.sysex_data_cb = user_callbacks.sysex_data_cb,
					.sysex_end_cb = user_callbacks.sysex_end_cb,
					.sysex_start_cb = user_callbacks.sysex_start_cb};
			usb_midi_parse_packet(packet.bytes, &parse_cb);
		} else {
			LOG_WRN("decoding USB MIDI packet failed with error %d", decode_result);
		}
		LOG_DBG_PACKET(packet);
		net_buf_reset(buf);
		int r = usbd_ep_enqueue(c_data, buf);
	} else if (bi->ep == 0x81) {
		// sent data
		net_buf_reset(buf);
		// int r = usbd_ep_enqueue(c_data, buf);
	}

	return 0;
}

/** USB power management handler suspended */
void usb_midi_suspended_cb(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
}

/** USB power management handler resumed */
void usb_midi_resumed_cb(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
}

/** Start of Frame */
static int sof_debug_ctr = 0;
void usb_midi_sof_cb(struct usbd_class_data *const c_data)
{
	// LOG_DBG("Instance %p", c_data);
	struct usb_midi_data *data = usbd_class_get_private(c_data);
	if (!ring_buf_is_empty(&data->tx_fifo)) {
		while (!ring_buf_is_empty(&data->tx_fifo)) {
			// Read 4 byte packets from the tx fifo and put them into 
			// the tx endpoint buffer
			uint8_t packet_bytes[4];
			int peek_result = ring_buf_get(&data->tx_fifo, packet_bytes, 4);
			net_buf_add_mem(data->tx_buf, packet_bytes, 4);
			// TODO: peek and then read if net_buf_add_mem succeeds
		}
		usbd_ep_enqueue(c_data, data->tx_buf);
	}
}

/** Class associated configuration is selected */
void usb_midi_enable_cb(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
	if (user_callbacks.available_cb) {
		user_callbacks.available_cb(1);
	}
	
	struct usb_midi_data *data = usbd_class_get_private(c_data);
	// TODO: don't hardcode endpoint addresses?
	if (data->rx_buf == NULL) {
		// Allocate buffer for receiving data
		data->rx_buf = usbd_ep_buf_alloc(c_data, 0x01, 64);
		// Enqueue the rx buffer. This signals to the stack that 
		// we're ready to receive data. If this is not done,
		// nothing will be received.
		int enqueue_result = usbd_ep_enqueue(c_data, data->rx_buf);
		if (enqueue_result != 0) {
			LOG_ERR("Failed to enqueue rx buf with error %d", enqueue_result);
		}
	}
	if (data->tx_buf == NULL) {
		data->tx_buf = usbd_ep_buf_alloc(c_data, 0x81, 64);
	}
}

/** Class associated configuration is disabled */
void usb_midi_disable_cb(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
	struct usb_midi_data *data = usbd_class_get_private(c_data);
	if (data->tx_buf) {
		int free_result = usbd_ep_buf_free(c_data->uds_ctx, data->tx_buf);
		if (free_result != 0) {
			LOG_ERR("Failed to free tx ep buf with error %d", free_result);
		} else {
			data->tx_buf = NULL;
		}
	}

	if (data->rx_buf) {
		int free_result = usbd_ep_buf_free(c_data->uds_ctx, data->rx_buf);
		if (free_result != 0) {
			LOG_ERR("Failed to free rx ep buf with error %d", free_result);
		} else {
			data->rx_buf = NULL;
		}
	}

	if (user_callbacks.available_cb) {
		user_callbacks.available_cb(0);
	}
}

/** Initialization of the class implementation */
int usb_midi_init_cb(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
	return 0;
}

/** Shutdown of the class implementation */
void usb_midi_shutdown_cb(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
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

struct usbd_class_api usb_midi_class_api = {
	.feature_halt = usb_midi_feature_halt_cb,
	.update = usb_midi_update_cb, 
	.request = usb_midi_request_cb,
	.suspended = usb_midi_suspended_cb,
	.resumed = usb_midi_resumed_cb,
	.sof = usb_midi_sof_cb,
	.enable = usb_midi_enable_cb,
	.disable = usb_midi_disable_cb,
	.init = usb_midi_init_cb,
	.shutdown = usb_midi_shutdown_cb,
	.get_desc = usb_midi_get_desc_cb
};

USBD_DEFINE_CLASS(usb_midi, &usb_midi_class_api, &usb_midi_class_data, NULL);

void usb_midi_init(struct usb_midi_cb_t *cb)
{
	user_callbacks.available_cb = cb->available_cb;
	user_callbacks.midi_message_cb = cb->midi_message_cb;
	user_callbacks.tx_done_cb = cb->tx_done_cb;
	user_callbacks.sysex_start_cb = cb->sysex_start_cb;
	user_callbacks.sysex_data_cb = cb->sysex_data_cb;
	user_callbacks.sysex_end_cb = cb->sysex_end_cb;
}