#include <zephyr/init.h>
#include <zephyr/usb/usbd.h>
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

// USBD_CLASS_DESCR_DEFINE(primary, 0)

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
	LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, OUT_JACK_PTR, (, )),
	LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, IN_JACK_PTR, (, )),
	(struct usb_desc_header *)&usb_midi_config_data.element,
	(struct usb_desc_header *)&usb_midi_config_data.in_ep,
	(struct usb_desc_header *)&usb_midi_config_data.in_cs_ep,
	(struct usb_desc_header *)&usb_midi_config_data.out_ep,
	(struct usb_desc_header *)&usb_midi_config_data.out_cs_ep,
	&nil_desc
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

#define USB_MIDI_VENDOR_REQ_OUT 0x5b
#define USB_MIDI_VENDOR_REQ_IN	0x5c

// #define LB_ISO_EP_MPS			256
// #define LB_ISO_EP_INTERVAL		1

/* Make supported vendor request visible for the device stack */
static const struct usbd_cctx_vendor_req usb_midi_vregs =
	USBD_VENDOR_REQ(USB_MIDI_VENDOR_REQ_OUT, USB_MIDI_VENDOR_REQ_IN);

/*
static void availability_changed(int is_available) {
	if (usb_midi_is_available == is_available) {
		return;
	}

	LOG_INF("device became %s ", is_available ? "available" : "unavailable");

	if (is_available) {
		temp_tx_buffer_size = 0;
	}
	if (user_callbacks.available_cb) {
		user_callbacks.available_cb(is_available);
	}
	usb_midi_is_available = is_available;
} */

/* #define USBD_MAX_POWER 250 // * 2
#define CONFIG_SAMPLE_USBD_SELF_POWERED 1
#define CONFIG_SAMPLE_USBD_REMOTE_WAKEUP 1

USBD_DEVICE_DEFINE(usb_device,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_USB_MIDI_DEVICE_VID, CONFIG_USB_MIDI_DEVICE_PID);

static const uint8_t attributes = (IS_ENABLED(CONFIG_SAMPLE_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

// Full speed configuration
USBD_CONFIGURATION_DEFINE(sample_fs_config,
			  attributes,
			  USBD_MAX_POWER, &fs_cfg_desc);

// High speed configuration
USBD_CONFIGURATION_DEFINE(sample_hs_config,
			  attributes,
			  USBD_MAX_POWER, &hs_cfg_desc); */


/* static void midi_out_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
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
			enum usb_midi_error_t error = usb_midi_packet_from_usb_bytes(buf, &packet);

			if (error != USB_MIDI_SUCCESS)
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
				if (error != USB_MIDI_SUCCESS)
				{
					LOG_ERR("Failed to parse packet with error %d", error);
				}
			}
		}
	} else {
		// printk("USB ep status %d\n", ep_status);
	}
}

static void midi_in_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	if (ep_status == USB_DC_EP_DATA_IN && user_callbacks.tx_done_cb)
	{
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
	// USB error reported by the controller
	case USB_DC_ERROR:
		LOG_DBG("USB_DC_ERROR");
		break;
	// USB reset
	case USB_DC_RESET:
		LOG_DBG("USB_DC_RESET");
		break;
	// USB connection established, hardware enumeration is completed
	case USB_DC_CONNECTED:
		LOG_DBG("USB_DC_CONNECTED");
		break;
	// USB configuration done
	case USB_DC_CONFIGURED:
		LOG_DBG("USB_DC_CONFIGURED");
		availability_changed(1);
		break;
	// USB connection lost
	case USB_DC_DISCONNECTED:
		LOG_DBG("USB_DC_DISCONNECTED");
		break;
	// USB connection suspended by the HOST
	case USB_DC_SUSPEND:
		availability_changed(0);
		break;
	// USB connection resumed by the HOST
	case USB_DC_RESUME:
		LOG_DBG("USB_DC_RESUME");
		break;
	// USB interface selected
	case USB_DC_INTERFACE:
		LOG_DBG("USB_DC_INTERFACE");
		break;
	// Set Feature ENDPOINT_HALT received
	case USB_DC_SET_HALT:
		LOG_DBG("USB_DC_SET_HALT");
		break;
	// Clear Feature ENDPOINT_HALT received
	case USB_DC_CLEAR_HALT:
		LOG_DBG("USB_DC_CLEAR_HALT");
		break;
	// Start of Frame received
	case USB_DC_SOF:
		LOG_DBG("USB_DC_SOF");
		break;
	// Initial USB connection status
	case USB_DC_UNKNOWN:
		LOG_DBG("USB_DC_UNKNOWN");
		break;
	}
} */

int usb_midi_tx(uint8_t cable_number, uint8_t *midi_bytes)
{
	/* struct usb_midi_packet_t packet;
	enum usb_midi_error_t error = usb_midi_packet_from_midi_bytes(midi_bytes, cable_number,
	&packet); if (error != USB_MIDI_SUCCESS)
	{
		LOG_ERR("Building packet from MIDI bytes %02x %02x %02x failed with error %d",
	midi_bytes[0], midi_bytes[1], midi_bytes[2], error); return -EINVAL;
	}
	LOG_DBG_PACKET(packet);
	int write_result = usb_write(0x81, packet.bytes, 4, NULL);
	return write_result; */
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

/* USBD_DEFINE_CFG_DATA(usb_midi_config) = {
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
}; */

struct usb_midi_data {
	struct usb_midi_config *const desc;
	const struct usb_desc_header **const fs_desc;
	const struct usb_desc_header **const hs_desc;
	atomic_t state;
};

static struct usb_midi_data usb_midi_data = {
	.desc = &usb_midi_config_data,
	.fs_desc = &xxx[0],
	.hs_desc = &xxx[0],
};

/** Feature halt state update handler */
void usb_midi_feature_halt(struct usbd_class_data *const c_data, uint8_t ep, bool halted)
{
	LOG_DBG("Instance %p, ep %u, halted %d",
		c_data, ep, halted);
}

/** Configuration update handler */
void usb_midi_update(struct usbd_class_data *const c_data, uint8_t iface, uint8_t alternate)
{
	LOG_DBG("Instance %p, interface %u alternate %u changed",
		c_data, iface, alternate);
}

/** USB control request handler to device */
int usb_midi_control_to_dev(struct usbd_class_data *const c_data,
			    const struct usb_setup_packet *const setup,
			    const struct net_buf *const buf)
{
	if (setup->RequestType.recipient != USB_REQTYPE_RECIPIENT_DEVICE) {
		errno = -ENOTSUP;
		return 0;
	}

	if (setup->bRequest == USB_MIDI_VENDOR_REQ_OUT) {
		LOG_DBG("Host-to-Device, wLength %u | %zu", setup->wLength, buf->len);
		// 	MIN(sizeof(lb_buf), buf->len));
		// memcpy(lb_buf, buf->data, MIN(sizeof(lb_buf), buf->len));
		return 0;
	}

	LOG_ERR("Class request 0x%x not supported", setup->bRequest);
	errno = -ENOTSUP;

	return 0;
}

/** USB control request handler to host */
int usb_midi_control_to_host(struct usbd_class_data *const c_data,
			     const struct usb_setup_packet *const setup, struct net_buf *const buf)
{
	if (setup->RequestType.recipient != USB_REQTYPE_RECIPIENT_DEVICE) {
		errno = -ENOTSUP;
		return 0;
	}

	if (setup->bRequest == USB_MIDI_VENDOR_REQ_IN) {
		// net_buf_add_mem(buf, lb_buf,
		//		MIN(sizeof(lb_buf), setup->wLength));

		LOG_DBG("Device-to-Host, wLength %u | %zu", setup->wLength, setup->wLength);

		return 0;
	}

	LOG_ERR("Class request 0x%x not supported", setup->bRequest);
	errno = -ENOTSUP;

	return 0;
}

/** Endpoint request completion event handler */
int usb_midi_request(struct usbd_class_data *const c_data, struct net_buf *buf, int err)
{
	/* struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct udc_buf_info *bi = NULL;

	bi = (struct udc_buf_info *)net_buf_user_data(buf);
	LOG_DBG("%p -> ep 0x%02x, len %u, err %d", c_data, bi->ep, buf->len, err);

	return usbd_ep_buf_free(uds_ctx, buf); */
	LOG_DBG("Instance %p, buf %p, err %d", c_data, buf, err);
	return 0;
}

/** USB power management handler suspended */
void usb_midi_suspended(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
}

/** USB power management handler resumed */
void usb_midi_resumed(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
}

/** Start of Frame */
void usb_midi_sof(struct usbd_class_data *const c_data)
{
	// LOG_DBG("Instance %p", c_data);
}

/** Class associated configuration is selected */
void usb_midi_enable(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
}

/** Class associated configuration is disabled */
void usb_midi_disable(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
}

/** Initialization of the class implementation */
int usb_midi_init(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
	return 0;
}

/** Shutdown of the class implementation */
void usb_midi_shutdown(struct usbd_class_data *const c_data)
{
	LOG_DBG("Instance %p", c_data);
}
 
/** Get function descriptor based on speed parameter */
void *usb_midi_get_desc(struct usbd_class_data *const c_data, const enum usbd_speed speed)
{
	LOG_DBG("Instance %p, speed %d", c_data, speed);

	struct usb_midi_data *data = usbd_class_get_private(c_data);

	if (speed == USBD_SPEED_HS) {
		return data->hs_desc;
	}

	return data->fs_desc;
}

struct usbd_class_api usb_midi_api = {
	.feature_halt = usb_midi_feature_halt,
	.update = usb_midi_update, 
	.control_to_dev = usb_midi_control_to_dev,
	.control_to_host = usb_midi_control_to_host,
	.request = usb_midi_request,
	.suspended = usb_midi_suspended,
	.resumed = usb_midi_resumed,
	.sof = usb_midi_sof,
	.enable = usb_midi_enable,
	.disable = usb_midi_disable,
	.init = usb_midi_init,
	.shutdown = usb_midi_shutdown,
	.get_desc = usb_midi_get_desc
};

USBD_DEFINE_CLASS(usb_midi, &usb_midi_api, &usb_midi_data, &usb_midi_vregs);

void usb_midi_register_callbacks(struct usb_midi_cb_t *cb)
{
	user_callbacks.available_cb = cb->available_cb;
	user_callbacks.midi_message_cb = cb->midi_message_cb;
	user_callbacks.tx_done_cb = cb->tx_done_cb;
	user_callbacks.sysex_start_cb = cb->sysex_start_cb;
	user_callbacks.sysex_data_cb = cb->sysex_data_cb;
	user_callbacks.sysex_end_cb = cb->sysex_end_cb;
	
}