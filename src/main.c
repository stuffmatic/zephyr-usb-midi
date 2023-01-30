#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/gpio.h>
#include <sys/ring_buffer.h>
#include "usb_midi/usb_midi.h"

/************************ App state ************************/

#define BUTTON_EVENT_RBUF_SIZE 128
RING_BUF_DECLARE(button_event_rbuf, BUTTON_EVENT_RBUF_SIZE);

#define SYSEX_TX_MESSAGE_SIZE 2000
#define TX_NOTE_NUMBER 0x69
#define TX_NOTE_VELOCITY 0x7f

struct sample_app_state_t {
	int usb_midi_is_available;
	int sysex_rx_data_byte_count;
	int sysex_tx_data_byte_count;
	int sysex_tx_in_progress;
	int button_states[4];
};

static struct sample_app_state_t sample_app_state = {
	.usb_midi_is_available = 0,
	.sysex_tx_data_byte_count = 0,
	.sysex_tx_in_progress = 0,
	.sysex_rx_data_byte_count = 0,
	.button_states = {
		0, 0, 0 ,0
	}
};

/************************ LEDs ************************/

#define LED_COUNT 3
/* Turn on LED 0 when USB MIDI is available */
#define LED_AVAILABLE 0
/* Toggle LED 1 on received non-sysex messages */
#define LED_RX_MSG 1
/* Toggle LED 2 on received sysex messages */
#define LED_RX_SYSEX 2

static const struct gpio_dt_spec leds[LED_COUNT] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios)
};

static void init_leds() {
	for (int i = 0; i < LED_COUNT; i++) {
		gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_ACTIVE);
		gpio_pin_set_dt(&leds[i], 0);
	}
}

/************************ Buttons ************************/

#define BUTTON_COUNT 4
#define BUTTON_TX_MSG_0 0
#define BUTTON_TX_MSG_1 1
#define BUTTON_TX_MSG_2 2
#define BUTTON_TX_SYSEX 3
static const struct gpio_dt_spec buttons[BUTTON_COUNT] = {
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0}),
		GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw1), gpios, {0}),
		GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw2), gpios, {0}),
		GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw3), gpios, {0}),
};
static struct gpio_callback button_cb_data;

static void button_pressed(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if ((pins & BIT(buttons[i].pin)) != 0) {
					sample_app_state.button_states[i] = !sample_app_state.button_states[i];
					int button_down = sample_app_state.button_states[i];
					int button_event_code = i | (button_down << 2);
					ring_buf_put(&button_event_rbuf, &button_event_code, 1);
        }
    }
}

static void init_buttons()
{
	for (int i = 0; i < BUTTON_COUNT; i++) {
			__ASSERT_NO_MSG(device_is_ready(buttons[i].port));
			int ret = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
			__ASSERT_NO_MSG(ret == 0);
			ret = gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_BOTH);
			__ASSERT_NO_MSG(ret == 0);
	}

	gpio_init_callback(
        &button_cb_data,
        button_pressed,
        BIT(buttons[0].pin) | BIT(buttons[1].pin) | BIT(buttons[2].pin) | BIT(buttons[3].pin));
    int ret = gpio_add_callback(buttons[0].port, &button_cb_data);
    __ASSERT_NO_MSG(ret == 0);
}

/****************** USB MIDI callbacks ******************/

static void midi_message_cb(uint8_t *bytes, uint8_t num_bytes, uint8_t cable_num)
{
	printk("rx non-sysex, cable %d: ", cable_num);
	for (int i = 0; i < num_bytes; i++) {
			printk("%02x ", bytes[i]);
	}
	printk("\n");
	gpio_pin_toggle_dt(&leds[LED_RX_MSG]);
}

static void sysex_start_cb(uint8_t cable_num)
{
	sample_app_state.sysex_rx_data_byte_count = 0;
}

static void sysex_data_cb(uint8_t* data_bytes, uint8_t num_data_bytes, uint8_t cable_num)
{
	sample_app_state.sysex_rx_data_byte_count += num_data_bytes;
}

static void sysex_end_cb(uint8_t cable_num)
{
	gpio_pin_toggle_dt(&leds[LED_RX_SYSEX]);
	printk("rx sysex, cable %d: %d data bytes\n", cable_num, sample_app_state.sysex_rx_data_byte_count);
}

static void usb_midi_available(bool is_available)
{
	sample_app_state.usb_midi_is_available = is_available;
	gpio_pin_set_dt(&leds[LED_AVAILABLE], is_available);
	if (!is_available) {
		gpio_pin_set_dt(&leds[LED_RX_MSG], 0);
		gpio_pin_set_dt(&leds[LED_RX_SYSEX], 0);
	}
}

/****************** Sample app ******************/

void next_sysex_chunk(uint8_t* chunk) {
	if (sample_app_state.sysex_tx_data_byte_count == 0) {
		chunk[0] = 0xf0;
		chunk[1] = 0x01;
		chunk[2] = 0x02;
	} else {
		for (int i = 0; i < 3; i++) {
			chunk[i] = sample_app_state.sysex_tx_data_byte_count;
			sample_app_state.sysex_tx_data_byte_count++;
		}
	}
}

void main(void)
{
	init_leds();
	init_buttons();

	/* Register USB MIDI callbacks */
	struct usb_midi_cb_t callbacks = {
	    .available_cb = usb_midi_available,
	    .midi_message_cb = midi_message_cb,
			.sysex_data_cb = sysex_data_cb,
			.sysex_end_cb = sysex_end_cb,
			.sysex_start_cb = sysex_start_cb
		};
	usb_midi_register_callbacks(&callbacks);

	/* Init USB */
	int enable_rc = usb_enable(NULL);
	__ASSERT(enable_rc == 0, "Failed to enable USB");

	while (1)
	{
		/* Poll button events */
		int button_event_code = 0;
		while (ring_buf_get(&button_event_rbuf, &button_event_code, 1)) {
			int button_idx = button_event_code & 0x3;
			int button_down = button_event_code >> 2;
			if (!sample_app_state.sysex_tx_in_progress) {
				if (button_idx <= BUTTON_TX_MSG_2) {
					uint8_t cable_num = button_idx;
					uint8_t msg[3] = { button_down ? 0x90 : 0x80, TX_NOTE_NUMBER, TX_NOTE_VELOCITY };
					usb_midi_tx(cable_num, msg);
				}
				else if (button_down) {
					uint8_t cable_num = 0;
					uint8_t msg[3] = { 0xf0, 0x11, 0xf7};
					// TODO: send large sysex
					usb_midi_tx(cable_num, msg);
					// printk("tx failed after %d byte\n", sample_app_state.sysex_tx_data_byte_count);
				}
			}
		}

		/* Sleep for 1 ms */
		k_msleep(1);
	}
}
