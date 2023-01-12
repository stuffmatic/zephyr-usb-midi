#ifndef ZEPHYR_USB_MIDI_H_
#define ZEPHYR_USB_MIDI_H_

#include <zephyr/zephyr.h>

/** A function to call when MIDI data has been received. */
typedef void (*usb_midi_rx_cb)(uint8_t cable_number, uint8_t* midi_bytes, uint8_t midi_byte_count);
/** A function to call when the USB MIDI device becomes available/unavailable. */
typedef void (*usb_midi_available_cb)(bool is_available);

struct usb_midi_handlers {
    usb_midi_rx_cb rx_cb;
    usb_midi_available_cb available_cb;
};

/**
 * Register callbacks to invoke when receiving MIDI data etc.
 */
void usb_midi_register_handlers(struct usb_midi_handlers* handlers);

/**
 * Send a MIDI event with a given cable number. The event must be 1, 2 or 3 bytes long passed
 * in a buffer of length 3 (unused bytes can be set to zero).
 * System real time messages must be sent as single byte events and must not be interleaved
 * in other events. All non-sysex events must start with a status byte (no running status).
 * Sysex sequences with more than three bytes must be split into smaller chunks by the caller.
 * The following sequences of sysex bytes are allowed, where d denotes a data byte.
 *
 * F0, F7
 * F0, d, F7
 * F0, d, d
 * d, d, d
 * d, d, F7
 * d, F7
 * F7
 *
 * @param cable_number Send the event on the virtual cable with this number.
 * Must be smaller than the number of outputs.
 * @param midi_bytes The MIDI bytes to send.
 */
uint32_t usb_midi_tx(uint8_t cable_number, uint8_t* midi_bytes);

#endif