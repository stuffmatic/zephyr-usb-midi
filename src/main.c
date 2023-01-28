#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/gpio.h>
#include "usb_midi/usb_midi.h"

#define SLEEP_TIME_MS 300
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static bool usb_midi_is_available = false;

static void log_buffer(const char* tag, uint8_t *bytes, uint8_t num_bytes)
{
	printk("%s", tag, num_bytes);
	for (int i = 0; i < num_bytes; i++) {
			printk("%02x ", bytes[i]);
	}
	printk("\n");
}

static void midi_message_cb(uint8_t *bytes, uint8_t num_bytes, uint8_t cable_num)
{
	log_buffer("midi msg", bytes, num_bytes);
	gpio_pin_toggle_dt(&led1);
}
static void midi_sysex_start_cb(uint8_t cable_num)
{
	log_buffer("sysex start", NULL, 0);
	gpio_pin_toggle_dt(&led1);
}
static void midi_sysex_data_cb(uint8_t* data_bytes, uint8_t num_data_bytes, uint8_t cable_num)
{
	log_buffer("sysex data", data_bytes, num_data_bytes);
	gpio_pin_toggle_dt(&led1);
}
void midi_sysex_end_cb(uint8_t cable_num)
{
	log_buffer("sysex end", NULL, 0);
	gpio_pin_toggle_dt(&led1);
}


void usb_midi_available(bool is_available)
{
	usb_midi_is_available = is_available;
	gpio_pin_set_dt(&led0, is_available);
	if (!is_available) {
		gpio_pin_set_dt(&led1, 0);
	}
}

void main(void)
{
	/* Set up LEDs */
	/* Turn on LED 0 when the device is online */
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&led0, 0);
	/* Toggle LED 1 on oncoming MIDI events */
	gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&led1, 0);

	/* Register USB MIDI handlers */
	struct usb_midi_cb_t handlers = {
	    .available_cb = usb_midi_available,
	    .midi_message_cb = midi_message_cb,
			.sysex_data_cb = midi_sysex_data_cb,
			.sysex_end_cb = midi_sysex_end_cb,
			.sysex_start_cb = midi_sysex_start_cb
		};
	usb_midi_register_handlers(&handlers);

	/* Init USB */
	int enable_rc = usb_enable(NULL);
	__ASSERT(enable_rc == 0, "Failed to enable USB");

	/* Send note on/off repeatedly on all virutal cables */
	bool is_note_on = false;
	uint8_t cable_num = 0;
	uint8_t note_num = 69;
	uint8_t note_vel = 127;

	while (1)
	{
		if (usb_midi_is_available)
		{
			/* Only send data if the device is available */
			uint8_t midi_bytes[3] = {is_note_on ? 0x90 : 0x80, note_num, note_vel};
			usb_midi_tx(cable_num, midi_bytes);
			if (!is_note_on) {
				/* Just sent note off. Move to the next virtual cable. */
				cable_num = (cable_num + 1) % CONFIG_USB_MIDI_NUM_OUTPUTS;
			}
			is_note_on = !is_note_on;
		}
		k_msleep(SLEEP_TIME_MS);
	}
}
