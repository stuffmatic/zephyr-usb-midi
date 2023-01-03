#ifndef ZEPHYR_USB_MIDI_H_
#define ZEPHYR_USB_MIDI_H_

#include <zephyr/init.h>

typedef void (*usb_midi_rx_cb)(uint8_t cable_number, uint8_t* midi_bytes, uint8_t midi_byte_count);
typedef void (*usb_midi_enabled_cb)(bool is_available);
struct usb_midi_handlers {
    usb_midi_rx_cb rx_cb;
    usb_midi_enabled_cb enabled_cb;
};

void usb_midi_register_handlers(struct usb_midi_handlers* handlers);
uint32_t usb_midi_tx(uint8_t cable_number, uint8_t* midi_bytes, uint8_t midi_byte_count);

#endif