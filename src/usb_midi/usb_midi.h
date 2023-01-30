#ifndef ZEPHYR_USB_MIDI_H_
#define ZEPHYR_USB_MIDI_H_

#include <stdint.h>
#include "usb_midi_packet.h"

/** A function to call when MIDI data has been received. */
typedef void (*usb_midi_packet_cb_t)(struct usb_midi_packet);
/** A function to call when the USB MIDI device becomes available/unavailable. */
typedef void (*usb_midi_available_cb_t)(bool is_available);

struct usb_midi_cb_t {
    usb_midi_available_cb_t available_cb;
    usb_midi_packet_cb_t packet_rx_cb;
    usb_midi_packet_cb_t packet_tx_cb;
    usb_midi_message_cb_t midi_message_cb;
    usb_midi_sysex_start_cb_t sysex_start_cb;
    usb_midi_sysex_data_cb_t sysex_data_cb;
    usb_midi_sysex_end_cb_t sysex_end_cb;
};

/**
 * Register callbacks to invoke when receiving MIDI messages etc.
 */
void usb_midi_register_callbacks(struct usb_midi_cb_t* handlers);

/**
 * Send a MIDI message with a given cable number. The event must be 1, 2 or 3 bytes long passed
 * in a buffer of length 3 (unused bytes can be set to zero).
 * All non-sysex messages must start with a status byte (no running status).
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
int usb_midi_tx(uint8_t cable_number, uint8_t* midi_bytes);

#endif