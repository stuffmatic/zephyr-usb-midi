#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/gpio.h>
#include "usb_midi.h"

#define SLEEP_TIME_MS 300
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static bool usb_midi_is_enabled = false;

void usb_midi_rx(uint8_t cable_number, uint8_t *midi_bytes, uint8_t midi_byte_count)
{
	gpio_pin_toggle_dt(&led1);
}

void usb_midi_enabled(bool is_enabled)
{
	usb_midi_is_enabled = is_enabled;
	gpio_pin_set_dt(&led0, is_enabled);
	if (!is_enabled) {
		gpio_pin_set_dt(&led1, 0);
	}
}

void main(void)
{
	// Set up LEDs
	// Turn on LED 0 when the device is online
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&led0, 0);
	// Toggle LED 1 on oncoming MIDI events
	gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&led1, 0);

	// Register USB MIDI handlers
	struct usb_midi_handlers handlers = {
	    .enabled_cb = usb_midi_enabled,
	    .rx_cb = usb_midi_rx};
	usb_midi_register_handlers(&handlers);

	// Init USB
	int enable_rc = usb_enable(NULL);
	__ASSERT(enable_rc == 0, "Failed to enable USB");

	// Send note on/off on channel 1 every second.
	bool is_note_on = false;
	uint8_t cable_num = 0;
	uint8_t note_num = 69;
	uint8_t note_vel = 127;

	while (1)
	{
		if (usb_midi_is_enabled)
		{ // Only send data if the device is available
			uint8_t midi_bytes[3] = {is_note_on ? 0x90 : 0x80, note_num, note_vel};
			usb_midi_tx(cable_num, midi_bytes, 3);
			is_note_on = !is_note_on;
			if (is_note_on) {
				cable_num = (cable_num + 1) % CONFIG_USB_MIDI_NUM_OUTPUTS;
			}
		}
		k_msleep(SLEEP_TIME_MS);
	}
}
