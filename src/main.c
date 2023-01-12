#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/gpio.h>
#include "usb_midi/usb_midi.h"

/****** USB MIDI packet tests start ******/
#include "usb_midi/usb_midi_packet.h"
void reset_packet(struct usb_midi_packet *packet)
{
	packet->bytes[0] = 0;
	packet->bytes[1] = 0;
	packet->bytes[2] = 0;
	packet->bytes[3] = 0;

	packet->cin = 0;
	packet->num_midi_bytes = 0;
	packet->cable_num = 0;
}

static void test_usb_midi_packet()
{
	struct usb_midi_packet packet;
	uint8_t cable_num = 7;

	for (uint8_t high_nibble = 0x8; high_nibble < 0xf; high_nibble++)
	{
		uint8_t msg[3] = {
			high_nibble << 4, 0x12, 0x23
		};
		reset_packet(&packet);
		uint32_t parse_rc = usb_midi_packet_from_midi_bytes(msg, cable_num, &packet);
		__ASSERT(parse_rc == 0, "Failed to parse MIDI message");
		__ASSERT(packet.cin == high_nibble, "Unexpected USB MIDI packet CIN");
		__ASSERT(packet.cable_num == cable_num, "Unexpected USB MIDI packet cable number");
	}
}
/****** USB MIDI packet tests end ******/

#define SLEEP_TIME_MS 300
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static bool usb_midi_is_available = false;

void usb_midi_rx(uint8_t cable_number, uint8_t *midi_bytes, uint8_t midi_byte_count)
{
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
	/* Run tests (should not be done here obvs) */
	test_usb_midi_packet();

	/* Set up LEDs */
	/* Turn on LED 0 when the device is online */
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&led0, 0);
	/* Toggle LED 1 on oncoming MIDI events */
	gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&led1, 0);

	/* Register USB MIDI handlers */
	struct usb_midi_handlers handlers = {
	    .available_cb = usb_midi_available,
	    .rx_cb = usb_midi_rx};
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
