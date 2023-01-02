#ifndef ZEPHYR_USB_MIDI_H_
#define ZEPHYR_USB_MIDI_H_

#include <zephyr/init.h>

typedef void (*usb_midi_rx_handler)(uint8_t cable_number, uint8_t* midi_bytes, uint8_t midi_byte_count);
void usb_midi_register_rx_handler(usb_midi_rx_handler handler);
uint32_t usb_midi_tx(uint8_t cable_number, uint8_t* midi_bytes, uint8_t midi_byte_count);

#endif