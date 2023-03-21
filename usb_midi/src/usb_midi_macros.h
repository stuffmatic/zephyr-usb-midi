#ifndef ZEPHYR_USB_MIDI_MACROS_H_
#define ZEPHYR_USB_MIDI_MACROS_H_

#include <zephyr/init.h>

/* Require at least one jack */
BUILD_ASSERT((CONFIG_USB_MIDI_NUM_INPUTS + CONFIG_USB_MIDI_NUM_OUTPUTS > 0), "USB MIDI device must have more than 0 jacks");

#ifdef CONFIG_USB_MIDI_USE_CUSTOM_JACK_NAMES

#define OUTPUT_JACK_STRING_DESCR_IDX(jack_idx) (4 + jack_idx)
#define INPUT_JACK_STRING_DESCR_IDX(jack_idx) (4 + jack_idx + CONFIG_USB_MIDI_NUM_OUTPUTS)

/* Define struct jack_string_descriptors holding all jack string descriptors. */
#define OUTPUT_JACK_STRING_DESCR(jack_number, _)                                               \
    struct output_jack_##jack_number##_string_descr_type                                       \
    {                                                                                          \
        uint8_t bLength;                                                                       \
        uint8_t bDescriptorType;                                                               \
        uint8_t bString[USB_BSTRING_LENGTH(CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME)]; \
    } __packed output_jack_##jack_number##_string_descr;

#define INPUT_JACK_STRING_DESCR(jack_number, _)                                               \
    struct input_jack_##jack_number##_string_descr_type                                       \
    {                                                                                         \
        uint8_t bLength;                                                                      \
        uint8_t bDescriptorType;                                                              \
        uint8_t bString[USB_BSTRING_LENGTH(CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME)]; \
    } __packed input_jack_##jack_number##_string_descr;

struct jack_string_descriptors
{
    LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, OUTPUT_JACK_STRING_DESCR, ())
    LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, INPUT_JACK_STRING_DESCR, ())
} _packed;

/* statically initialize a struct jack_string_descriptors */
#define INIT_INPUT_JACK_STRING_DESCR(jack_number, _)                                              \
    .input_jack_##jack_number##_string_descr = {                                                  \
        .bLength = USB_STRING_DESCRIPTOR_LENGTH(CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME), \
        .bDescriptorType = USB_DESC_STRING,                                                       \
        .bString = CONFIG_USB_MIDI_INPUT_JACK_##jack_number##_NAME},

#define INIT_OUTPUT_JACK_STRING_DESCR(jack_number, _)                                              \
    .output_jack_##jack_number##_string_descr = {                                                  \
        .bLength = USB_STRING_DESCRIPTOR_LENGTH(CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME), \
        .bDescriptorType = USB_DESC_STRING,                                                        \
        .bString = CONFIG_USB_MIDI_OUTPUT_JACK_##jack_number##_NAME},

USBD_STRING_DESCR_USER_DEFINE(primary)
struct jack_string_descriptors jack_string_desc = {
    LISTIFY(CONFIG_USB_MIDI_NUM_OUTPUTS, INIT_OUTPUT_JACK_STRING_DESCR, ())
        LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, INIT_INPUT_JACK_STRING_DESCR, ())};
#else
/* No jack string descriptors by default  */
#define INPUT_JACK_STRING_DESCR_IDX(jack_idx) 0
#define OUTPUT_JACK_STRING_DESCR_IDX(jack_idx) 0
#endif /* CONFIG_USB_MIDI_USE_CUSTOM_JACK_NAMES */

/* Audio control interface descriptor */
#define INIT_AC_IF                                                      \
    {                                                                   \
        .bLength = sizeof(struct usb_if_descriptor),                    \
        .bDescriptorType = USB_DESC_INTERFACE,                          \
        .bInterfaceNumber = 0,                                          \
        .bAlternateSetting = 0,                                         \
        .bNumEndpoints = 0,                                             \
        .bInterfaceClass = USB_MIDI_AUDIO_INTERFACE_CLASS,              \
        .bInterfaceSubClass = USB_MIDI_AUDIOCONTROL_INTERFACE_SUBCLASS, \
        .bInterfaceProtocol = 0x00,                                     \
        .iInterface = 0x00                                              \
    }

/* Class specific audio control interface descriptor */
#define INIT_AC_CS_IF                                             \
    {                                                             \
        .bLength = sizeof(struct usb_midi_ac_if_descriptor),      \
        .bDescriptorType = USB_DESC_CS_INTERFACE,                 \
        .bDescriptorSubtype = 0x01,                               \
        .bcdADC = 0x0100,                                         \
        .wTotalLength = sizeof(struct usb_midi_ac_if_descriptor), \
        .bInCollection = 0x01,                                    \
        .baInterfaceNr = 0x01                                     \
    }

/* MIDI streaming interface descriptor */
#define INIT_MS_IF                                                       \
    {                                                                    \
        .bLength = sizeof(struct usb_if_descriptor),                     \
        .bDescriptorType = USB_DESC_INTERFACE,                           \
        .bInterfaceNumber = 0x01,                                        \
        .bAlternateSetting = 0x00,                                       \
        .bNumEndpoints = 2,                                              \
        .bInterfaceClass = USB_MIDI_AUDIO_INTERFACE_CLASS,               \
        .bInterfaceSubClass = USB_MIDI_MIDISTREAMING_INTERFACE_SUBCLASS, \
        .bInterfaceProtocol = 0x00,                                      \
        .iInterface = 0x00                                               \
    }

/* Class specific MIDI streaming interface descriptor */
#define INIT_MS_CS_IF                                        \
    {                                                        \
        .bLength = sizeof(struct usb_midi_ms_if_descriptor), \
        .bDescriptorType = USB_DESC_CS_INTERFACE,            \
        .bDescriptorSubtype = 0x01,                          \
        .BcdADC = 0x0100,                                    \
        .wTotalLength = MIDI_MS_IF_DESC_TOTAL_SIZE           \
    }

/* Embedded MIDI input jack */
#define INIT_IN_JACK(idx, idx_offset)                          \
    {                                                          \
        .bLength = sizeof(struct usb_midi_in_jack_descriptor), \
        .bDescriptorType = USB_DESC_CS_INTERFACE,              \
        .bDescriptorSubtype = USB_MIDI_IF_DESC_MIDI_IN_JACK,   \
        .bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,              \
        .bJackID = 1 + idx + idx_offset,                       \
        .iJack = INPUT_JACK_STRING_DESCR_IDX(idx),             \
    }

/* Embedded MIDI output jack */
#define INIT_OUT_JACK(idx, jack_id_idx_offset)                  \
    {                                                           \
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

/* Out endpoint */
#define INIT_OUT_EP                                         \
    {                                                       \
        .bLength = sizeof(struct usb_ep_descriptor_padded), \
        .bDescriptorType = USB_DESC_ENDPOINT,               \
        .bEndpointAddress = 0x01,                           \
        .bmAttributes = 0x02,                               \
        .wMaxPacketSize = 0x0040,                           \
        .bInterval = 0x00,                                  \
        .bRefresh = 0x00,                                   \
        .bSynchAddress = 0x00,                              \
    }

/* In endpoint */
#define INIT_IN_EP                                          \
    {                                                       \
        .bLength = sizeof(struct usb_ep_descriptor_padded), \
        .bDescriptorType = USB_DESC_ENDPOINT,               \
        .bEndpointAddress = 0x81,                           \
        .bmAttributes = 0x02,                               \
        .wMaxPacketSize = 0x0040,                           \
        .bInterval = 0x00,                                  \
        .bRefresh = 0x00,                                   \
        .bSynchAddress = 0x00,                              \
    }

#define ELEMENT_ID 0xf0
#define IDX_WITH_OFFSET(index, offset) (index + offset)
#define INIT_INPUT_PIN(index, offset)   \
    {                                   \
        .baSourceID = (index + offset), \
        .baSourcePin = 1                \
    }

#define INIT_ELEMENT                                                                                                                                   \
    {                                                                                                                                                  \
        .bLength = sizeof(struct usb_midi_element_descriptor),                                                                                         \
        .bDescriptorType = USB_DESC_CS_INTERFACE,                                                                                                      \
        .bDescriptorSubtype = USB_MIDI_IF_DESC_ELEMENT,                                                                                                \
        .bElementID = ELEMENT_ID,                                                                                                                      \
        .bNrInputPins = CONFIG_USB_MIDI_NUM_INPUTS,                                                                                                    \
        .input_pins = {                                                                                                                                \
            LISTIFY(CONFIG_USB_MIDI_NUM_INPUTS, INIT_INPUT_PIN, (, ), 1)},                                                                             \
        .bNrOutputPins = CONFIG_USB_MIDI_NUM_OUTPUTS, .bInTerminalLink = 0, .bOutTerminalLink = 0, .bElCapsSize = 1, .bmElementCaps = 1, .iElement = 0 \
    }

/* Value for the wTotalLength field of the class-specific MS Interface Descriptor,
     i.e the total number of bytes following that descriptor. */
#define MIDI_MS_IF_DESC_TOTAL_SIZE                                                  \
    (                                                                               \
        sizeof(struct usb_midi_in_jack_descriptor) * CONFIG_USB_MIDI_NUM_INPUTS +   \
        sizeof(struct usb_midi_out_jack_descriptor) * CONFIG_USB_MIDI_NUM_OUTPUTS + \
        sizeof(struct usb_midi_element_descriptor) +                                \
        sizeof(struct usb_ep_descriptor_padded) +                                   \
        sizeof(struct usb_midi_bulk_out_ep_descriptor) +                            \
        sizeof(struct usb_ep_descriptor_padded) +                                   \
        sizeof(struct usb_midi_bulk_in_ep_descriptor))

#endif /* ZEPHYR_USB_MIDI_MACROS_H_ */