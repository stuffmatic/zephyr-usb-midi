#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include "usb_midi.h"

#define SLEEP_TIME_MS 1000

void usb_midi_rx(uint8_t cable_number, uint8_t *midi_bytes, uint8_t midi_byte_count)
{
	printk("usb_midi_rx cable_number %d\n", cable_number);
}

void usb_midi_enabled(bool is_enabled)
{
	printk("usb_midi_enabled %d\n", is_enabled);
}

void main(void)
{
	struct usb_midi_handlers handlers = {
	    .enabled_cb = usb_midi_enabled,
	    .rx_cb = usb_midi_rx};
	usb_midi_register_handlers(&handlers);

	int enable_rc = usb_enable(NULL);
	printk("enable_rc %d\n", enable_rc);

	int is_note_on = 1;
	while (1)
	{
		k_msleep(SLEEP_TIME_MS);
		uint8_t midi_bytes[3] = {is_note_on ? 0x90 : 0x80, 69, 127};
		// printk("sending bytes\n");
		usb_midi_tx(1, midi_bytes, 3);
		is_note_on = !is_note_on;
	}
}
