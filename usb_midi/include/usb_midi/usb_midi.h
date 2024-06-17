#ifndef ZEPHYR_USB_MIDI_H_
#define ZEPHYR_USB_MIDI_H_

#include <stdint.h>

/** A function to call when the USB MIDI device becomes available/unavailable. */
typedef void (*usb_midi_available_cb_t)(int is_available);
/** A function to call when a USB MIDI packet has just been sent. */
typedef void (*usb_midi_tx_done_cb_t)();
/** A function to call when a non-sysex message has been received. */
typedef void (*usb_midi_message_cb_t)(uint8_t *bytes, uint8_t num_bytes, uint8_t cable_num);
/** A function to call when a sysex message starts */
typedef void (*usb_midi_sysex_start_cb_t)(uint8_t cable_num);
/** A function to call when sysex data bytes have been received */
typedef void (*usb_midi_sysex_data_cb_t)(uint8_t* data_bytes, uint8_t num_data_bytes, uint8_t cable_num);
/** A function to call when a sysex message ends */
typedef void (*usb_midi_sysex_end_cb_t)(uint8_t cable_num);

struct usb_midi_cb_t {
    usb_midi_available_cb_t available_cb;
    usb_midi_tx_done_cb_t tx_done_cb;
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
 * Send a MIDI message with a given cable number. The event must be 1, 2 or 3 
 * bytes long passed in a buffer of length 3 (unused bytes can be set to zero).
 * All non-sysex messages must start with a status byte (no running status).
 * 
 * Sysex messages with more than three bytes must be split into smaller chunks 
 * by the caller. Below is a list of allowed sysex chunk types that can be combined
 * to form any valid sysex message. Data bytes are denoted by d.
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
 * @return 0 on success, a non-zero number on failure.
 */
int usb_midi_tx(uint8_t cable_number, uint8_t* midi_bytes);

/**
 * Enqueue a message for transmission. Used to send more than one
 * message per USB tx packet, which is useful for increasing throughput.
 * @return 0 if the message was enqueued, otherwise a non-zero number indicating that
 * usb_midi_tx_buffer_send should be called.
 */
int usb_midi_tx_buffer_add(uint8_t cable_number, uint8_t* midi_bytes);

/**
 * Indicates if more messages can be enqueued for transmission.
 * @return Zero if more messages can be enqueued. A non-zero number indicates that 
 * usb_midi_tx_buffer_send should be called.
 */
int usb_midi_tx_buffer_is_full();

/**
 * Send enqueued messages, if any, in a single USB packet.
 */
int usb_midi_tx_buffer_send();

#endif