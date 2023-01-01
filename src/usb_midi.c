#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>

struct usb_midi_ac_if_descriptor {
	uint8_t bLength; // 1 0x09 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; // 1 0x24 CS_INTERFACE.
    uint8_t bDescriptorSubtype; // 1 0x01 HEADER subtype.
    uint16_t bcdADC; // 2 0x0100 Revision of class specification - 1.0
    uint16_t wTotalLength; // 2 0x0009 Total size of class specific descriptors.
    uint8_t bInCollection; // 1 0x01 Number of streaming interfaces.
    uint8_t baInterfaceNr; // 1 0x01 MIDIStreaming interface 1 belongs to this AudioControl interface.
} __packed;

#define INIT_AC_CS_IF(num_streaming_ifs, interface_nr) \
    {                                                               \
        .bLength = sizeof(struct usb_midi_ac_if_descriptor), \
        .bDescriptorType = USB_DESC_CS_INTERFACE, \
        .bDescriptorSubtype = 0x01, \
        .bcdADC = 0x0100, \
        .wTotalLength = 0x0009, \
        .bInCollection = num_streaming_ifs, \
        .baInterfaceNr = interface_nr \
    }

struct usb_midi_ms_if_descriptor {
	uint8_t bLength; // 1 0x07 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; // 1 0x24 CS_INTERFACE descriptor.
    uint8_t bDescriptorSubtype; // 1 0x01 MS_HEADER subtype.
    uint16_t BcdADC; // 2 0x0100 Revision of this class specification.
    uint16_t wTotalLength; // 2 0x0041 Total size of class-specific descriptors .
} __packed;

#define INIT_MS_CS_IF(total_length) \
    { \
        .bLength = sizeof(struct usb_midi_ms_if_descriptor), \
        .bDescriptorType = USB_DESC_CS_INTERFACE, \
        .bDescriptorSubtype = 0x01, \
        .BcdADC = 0x0100, \
        .wTotalLength = total_length \
    }

struct usb_midi_in_jack_descriptor {
    uint8_t bLength; // 1 0x06 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; // 1 0x24 CS_INTERFACE descriptor.
    uint8_t bDescriptorSubtype; // 1 0x02 MIDI_IN_JACK subtype.
    uint8_t bJackType; // 1 0x01 EMBEDDED.
    uint8_t bJackID; // 1 0x01 ID of this Jack.
    uint8_t iJack; // 1 0x00 Unused
} __packed;

#define INIT_IN_JACK(jack_id) \
    { \
        .bLength = sizeof(struct usb_midi_in_jack_descriptor), \
        .bDescriptorType = USB_DESC_CS_INTERFACE, \
        .bDescriptorSubtype = 0x02, \
        .bJackType = 0x01, \
        .bJackID = jack_id, \
        .iJack = 0, \
    }

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

#define INIT_OUT_JACK(jack_id, num_input_pins, source_id, source_pin) \
    { \
        .bLength = sizeof(struct usb_midi_out_jack_descriptor), \
        .bDescriptorType = USB_DESC_CS_INTERFACE, \
        .bDescriptorSubtype = 0x03, \
        .bJackType = 0x01, \
        .bJackID = jack_id, \
        .bNrInputPins = num_input_pins, \
        .BaSourceID = source_id, \
        .BaSourcePin = source_pin, \
        .iJack = 0x00 \
    }

struct usb_midi_bulk_out_ep_descriptor {
    uint8_t bLength; // 1 0x05 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; // 1 0x25 CS_ENDPOINT descriptor
    uint8_t bDescriptorSubtype; // 1 0x01 MS_GENERAL subtype.
    uint8_t bNumEmbMIDIJack; // 1 0x01 Number of embedded MIDI IN Jacks.
    uint8_t BaAssocJackID; // (1) 1 0x01 ID of the Embedded MIDI IN Jack.
} __packed;

#define INIT_OUT_EP(num_embedded_in_jacks, embedded_in_jack_id) \
    { \
        .bLength = sizeof(struct usb_midi_bulk_out_ep_descriptor), \
        .bDescriptorType = USB_DESC_CS_ENDPOINT,  \
        .bDescriptorSubtype = 0x01, \
        .bNumEmbMIDIJack = num_embedded_in_jacks, \
        .BaAssocJackID = embedded_in_jack_id \
    }

struct usb_midi_bulk_in_ep_descriptor {
    uint8_t bLength; //  1 0x05 Size of this descriptor, in bytes.
    uint8_t bDescriptorType; //  1 0x25 CS_ENDPOINT descriptor
    uint8_t bDescriptorSubtype; //  1 0x01 MS_GENERAL subtype.
    uint8_t bNumEmbMIDIJack; //  1 0x01 Number of embedded MIDI OUT Jacks.
    uint8_t BaAssocJackID; // (1) 1 0x03 ID of the Embedded MIDI OUT Jack.
} __packed;

#define INIT_IN_EP(num_embedded_out_jacks, embedded_out_jack_id) \
    { \
        .bLength = sizeof(struct usb_midi_bulk_in_ep_descriptor), \
        .bDescriptorType = USB_DESC_CS_ENDPOINT, \
        .bDescriptorSubtype = 0x01, \
        .bNumEmbMIDIJack = num_embedded_out_jacks, \
        .BaAssocJackID = embedded_out_jack_id \
    }

#define INIT_DEVICE_DESC \
    { \
        .bLength = sizeof(struct usb_device_descriptor), \
	    .bDescriptorType = USB_DESC_DEVICE, \
        .bcdUSB = 0x0110, \
        .bDeviceClass = 0x00, \
	    .bDeviceSubClass = 0x00, \
	    .bDeviceProtocol = 0x00, \
	    .bMaxPacketSize0 = 64, /* 0x08 */ \
	    .idVendor = 0x2FE3, /* TODO: set properly */ \
	    .idProduct = 0x1, /* TODO: set properly */ \
	    .bcdDevice = 0x0, /* TODO: set properly */ \
	    .iManufacturer = 0x1, /* TODO: set properly */  \
	    .iProduct = 0x2, /* TODO: set properly */ \
	    .iSerialNumber = 0x0, /* TODO: set properly */ \
	    .bNumConfigurations = 0x1 \
    }

#define INIT_CFG_DESC(total_length) \
    { \
        .bLength = sizeof(struct usb_cfg_descriptor), \
	    .bDescriptorType = USB_DESC_CONFIGURATION , \
	    .wTotalLength = total_length, \
	    .bNumInterfaces = 0x02, \
	    .bConfigurationValue = 0x01, \
	    .iConfiguration = 0x00, \
	    .bmAttributes = 0x80, /* TODO: set properly */ \
	    .bMaxPower = 0x32, /* TODO: set properly */ \
    }


struct usb_midi_config {
	struct usb_device_descriptor dev; // size 18
    // Size below: 39 + 9 + 9 + 9 + 9 + 7
	struct usb_cfg_descriptor cfg; // size 9
	struct usb_if_descriptor ac_if; // size 9
    struct usb_midi_ac_if_descriptor ac_cs_if; // size 9
    struct usb_if_descriptor ms_if; // size 9
    struct usb_midi_ms_if_descriptor ms_cs_if; // size 7
    // Size below: 39
    struct usb_midi_in_jack_descriptor in_jack; // size 6
    struct usb_midi_out_jack_descriptor out_jack; // size 9
    struct usb_ep_descriptor out_ep;  // size 7 (NOTE: Says 9 in the spec example)
    struct usb_midi_bulk_out_ep_descriptor out_cs_ep; // size 5
    struct usb_ep_descriptor in_ep; // size 7 (NOTE: Says 9 in the spec example)
    struct usb_midi_bulk_in_ep_descriptor in_cs_ep; // size 5
} __packed;

#define NUM_ENDPOINTS 2
#define MIDI_MS_IF_DESC_TOTAL_SIZE 39
#define CFG_TOTAL_LENGTH (MIDI_MS_IF_DESC_TOTAL_SIZE + 9 + 9 + 9 + 9 + 7)

USBD_DEVICE_DESCR_DEFINE(primary) struct usb_midi_config usb_midi_device_descr = {
    .dev = INIT_DEVICE_DESC,
    .cfg = INIT_CFG_DESC(CFG_TOTAL_LENGTH),
    .ac_if = {
        .bLength = sizeof(struct usb_if_descriptor),
	    .bDescriptorType = USB_DESC_INTERFACE,
	    .bInterfaceNumber = 0,
	    .bAlternateSetting = 0,
	    .bNumEndpoints = 0,
	    .bInterfaceClass = 0x01, // TODO: Constant
	    .bInterfaceSubClass = 0x01, // TODO: Constant
	    .bInterfaceProtocol = 0x00,
	    .iInterface = 0x00
    },
    .ac_cs_if = INIT_AC_CS_IF(0x01, 0x01),
    .ms_if = {
        .bLength = sizeof(struct usb_if_descriptor),
	    .bDescriptorType = USB_DESC_INTERFACE,
	    .bInterfaceNumber = 0x01,
	    .bAlternateSetting = 0x00,
	    .bNumEndpoints = NUM_ENDPOINTS,
	    .bInterfaceClass = 0x01, // TODO: Constant
	    .bInterfaceSubClass = 0x03, // TODO: Constant
	    .bInterfaceProtocol = 0x00,
	    .iInterface = 0x00
    },
    .ms_cs_if = INIT_MS_CS_IF(MIDI_MS_IF_DESC_TOTAL_SIZE),
    .in_jack = INIT_IN_JACK(0x01),
    .out_jack = INIT_OUT_JACK(0x02, 0x01, 0x02, 0x01),
    .out_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
	    .bDescriptorType = USB_DESC_ENDPOINT,
	    .bEndpointAddress = 0x01,
	    .bmAttributes = 0x02,
	    .wMaxPacketSize = 0x0040,
	    .bInterval = 0x00
    },
    .out_cs_ep = INIT_OUT_EP(0x01, 0x01),
    .in_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
	    .bDescriptorType = USB_DESC_ENDPOINT,
	    .bEndpointAddress = 0x81,
	    .bmAttributes = 0x02,
	    .wMaxPacketSize = 0x0040,
	    .bInterval = 0x00
    },
    .in_cs_ep = INIT_IN_EP(0x01, 0x02),
};

static void midi_out_cb(uint8_t ep, enum usb_dc_ep_cb_status_code
ep_status)
{
	uint32_t bytes_to_read;

	// usb_read(ep, NULL, 0, &bytes_to_read);
	// LOG_DBG("ep 0x%x, bytes to read %d ", ep, bytes_to_read);
	// usb_read(ep, loopback_buf, bytes_to_read, NULL);
}

static void midi_in_cb(uint8_t ep, enum usb_dc_ep_cb_status_code
ep_status)
{
	/* if (usb_write(ep, loopback_buf, CONFIG_LOOPBACK_BULK_EP_MPS,
NULL)) {
		LOG_DBG("ep 0x%x", ep);
	} */
}

static struct usb_ep_cfg_data midi_ep_cfg[] = {
	{
		.ep_cb = midi_in_cb,
		.ep_addr = 0x81,
	},
    {
		.ep_cb = midi_out_cb,
		.ep_addr = 0x01,
	},
};

void usb_status_callback(enum usb_dc_status_code cb_status, const uint8_t *param)
{
    switch (cb_status)
    {
    /** USB error reported by the controller */
    case USB_DC_ERROR:
        printk("USB_DC_ERROR\n");
        break;
    /** USB reset */
    case USB_DC_RESET:
        printk("USB_DC_RESET\n");
        break;
    /** USB connection established, hardware enumeration is completed */
    case USB_DC_CONNECTED:
        printk("USB_DC_CONNECTED\n");
        break;
    /** USB configuration done */
    case USB_DC_CONFIGURED:
        printk("USB_DC_CONFIGURED\n");
        break;
    /** USB connection lost */
    case USB_DC_DISCONNECTED:
        printk("USB_DC_DISCONNECTED\n");
        break;
    /** USB connection suspended by the HOST */
    case USB_DC_SUSPEND:
        printk("USB_DC_SUSPEND\n");
        break;
    /** USB connection resumed by the HOST */
    case USB_DC_RESUME:
        printk("USB_DC_RESUME\n");
        break;
    /** USB interface selected */
    case USB_DC_INTERFACE:
        printk("USB_DC_INTERFACE\n");
        break;
    /** Set Feature ENDPOINT_HALT received */
    case USB_DC_SET_HALT:
        printk("USB_DC_SET_HALT\n");
        break;
    /** Clear Feature ENDPOINT_HALT received */
    case USB_DC_CLEAR_HALT:
        printk("USB_DC_CLEAR_HALT\n");
        break;
    /** Start of Frame received */
    case USB_DC_SOF:
        printk("USB_DC_SOF\n");
        break;
    /** Initial USB connection status */
    case USB_DC_UNKNOWN:
        printk("USB_DC_UNKNOWN\n");
        break;
    }
}

USBD_DEFINE_CFG_DATA(usb_midi_config) = {
	.usb_device_description = &usb_midi_device_descr,
	.interface_config = NULL,
	.interface_descriptor = &usb_midi_device_descr.ac_if,
	.cb_usb_status = usb_status_callback,
	.interface = {
		.class_handler = NULL,
		.custom_handler = NULL,
		.vendor_handler = NULL, // loopback_vendor_handler,
	},
	.num_endpoints = ARRAY_SIZE(midi_ep_cfg),
	.endpoint = midi_ep_cfg,
};