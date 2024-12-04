#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/gpio.h>
#include <usb_midi/usb_midi.h>
#if defined(CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT) || NRF_CLOCK_HAS_HFCLK192M
#include <nrfx_clock.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usb_midi_sample);

struct k_work button_press_work;
struct k_work event_tx_work;
struct k_work_delayable rx_led_off_work;
struct k_work_delayable tx_led_off_work;
static void send_next_sysex_chunk();

/************************ App state ************************/
struct sample_app_state_t {
	int usb_midi_is_available;
	int tx_note_off;
	
	int sysex_rx_byte_count;
	uint8_t sysex_rx_bytes[CONFIG_SYSEX_ECHO_MAX_LENGTH];
	int64_t sysex_rx_start_time;

	int sysex_tx_is_echo;
	int sysex_tx_byte_count;
	int sysex_tx_msg_size;
	int sysex_tx_in_progress;
	int sysex_tx_cable_num;
	int64_t sysex_tx_start_time;
};

static struct sample_app_state_t sample_app_state = {.usb_midi_is_available = 0,
							 .sysex_rx_byte_count = 0,
							 .sysex_tx_in_progress = 0,
							 .sysex_rx_start_time = 0,
						     .sysex_tx_byte_count = 0,
							 .sysex_tx_start_time = 0,
							 .sysex_tx_is_echo = 0,
						     .sysex_tx_in_progress = 0,
							 .sysex_tx_cable_num = 0,
						     .tx_note_off = 0,
							 .sysex_rx_byte_count = 0
							 };

static void log_sysex_transfer_time(int is_tx, int cable_num, int num_bytes, int time_ms) {
	float bytes_per_s = time_ms == 0 ? 0 : (float)num_bytes / (0.001 * time_ms);
	LOG_INF("sysex %s done | cable %d | %d bytes in %d ms | %d bytes/s", is_tx ? "tx" : "rx", cable_num,
		num_bytes, (int)time_ms, (int)bytes_per_s);
}

static void sysex_tx_will_start(int is_echo, int msg_size, int cable_num) {
	__ASSERT_NO_MSG(sample_app_state.sysex_tx_in_progress == 0);
	sample_app_state.sysex_tx_in_progress = 1;
	sample_app_state.sysex_tx_is_echo = is_echo;
	sample_app_state.sysex_tx_byte_count = 0;
	sample_app_state.sysex_tx_msg_size = msg_size;
	sample_app_state.sysex_tx_cable_num = cable_num;
	sample_app_state.sysex_tx_start_time = k_uptime_get();
}

/************************ LEDs ************************/
static struct gpio_dt_spec usb_midi_available_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static struct gpio_dt_spec midi_rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static struct gpio_dt_spec midi_tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

static void init_leds()
{
	gpio_pin_configure_dt(&usb_midi_available_led, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&usb_midi_available_led, 0);

	gpio_pin_configure_dt(&midi_rx_led, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&midi_rx_led, 0);

	gpio_pin_configure_dt(&midi_tx_led, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&midi_tx_led, 0);
}

static void set_usb_midi_available_led(int is_available)
{
	gpio_pin_set_dt(&usb_midi_available_led, is_available);
}

static void flash_tx_led()
{
	gpio_pin_set_dt(&midi_tx_led, 1);
	k_work_cancel_delayable(&tx_led_off_work);
	k_work_schedule(&tx_led_off_work, Z_TIMEOUT_MS(CONFIG_LED_FLASH_DURATION_MS));
}

static void flash_rx_led()
{
	gpio_pin_set_dt(&midi_rx_led, 1);
	k_work_cancel_delayable(&rx_led_off_work);
	k_work_schedule(&rx_led_off_work, Z_TIMEOUT_MS(CONFIG_LED_FLASH_DURATION_MS));
}

/****************** Work queue callbacks ******************/

void on_event_tx(struct k_work *item)
{
	if (sample_app_state.usb_midi_is_available && !sample_app_state.sysex_tx_in_progress) {
		uint8_t note = CONFIG_TX_PERIODIC_NOTE_NUMBER;
		uint8_t vel = CONFIG_TX_PERIODIC_NOTE_VELOCITY;
		uint8_t msg[3] = {sample_app_state.tx_note_off ? 0x80 : 0x90, note, vel };
		flash_tx_led();
		usb_midi_tx(0, msg);
		sample_app_state.tx_note_off = !sample_app_state.tx_note_off;
	}
}

void on_button_press(struct k_work *item)
{
	if (sample_app_state.usb_midi_is_available && !sample_app_state.sysex_tx_in_progress) {
		sysex_tx_will_start(0, CONFIG_SYSEX_TX_TEST_MSG_SIZE, CONFIG_SYSEX_TX_TEST_MSG_CABLE_NUM);
		send_next_sysex_chunk();
	}
}

void on_rx_led_off(struct k_work *item)
{
	gpio_pin_set_dt(&midi_rx_led, 0);
}

void on_tx_led_off(struct k_work *item)
{
	gpio_pin_set_dt(&midi_tx_led, 0);
}

/************************ Buttons ************************/

static struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
static struct gpio_callback button_cb_data;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	k_work_submit(&button_press_work);
}

static void init_button()
{
	__ASSERT_NO_MSG(device_is_ready(button.port));
	int ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	__ASSERT_NO_MSG(ret == 0);
	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	__ASSERT_NO_MSG(ret == 0);

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	ret = gpio_add_callback(button.port, &button_cb_data);
	__ASSERT_NO_MSG(ret == 0);
}

/****************** USB MIDI callbacks ******************/

static void midi_message_cb(uint8_t *bytes, uint8_t num_bytes, uint8_t cable_num)
{
	flash_rx_led();
}

static void sysex_start_cb(uint8_t cable_num)
{
	sample_app_state.sysex_rx_start_time = k_uptime_get();
	sample_app_state.sysex_rx_byte_count = 0;
	sample_app_state.sysex_rx_bytes[0] = 0xf0; 
	sample_app_state.sysex_rx_byte_count++;
	flash_rx_led();
}

static void sysex_data_cb(uint8_t *data_bytes, uint8_t num_data_bytes, uint8_t cable_num)
{
#ifdef CONFIG_SYSEX_ECHO_ENABLED
	for (int i = 0; i < num_data_bytes; i++) {
		int dest_idx = sample_app_state.sysex_rx_byte_count + i;
		if (dest_idx == CONFIG_SYSEX_ECHO_MAX_LENGTH - 1) {
			// reached max length. end stored message prematurely by adding
			// end of sysex status byte.
			sample_app_state.sysex_rx_bytes[dest_idx] = 0xf7;
		}
		else if (dest_idx >= CONFIG_SYSEX_ECHO_MAX_LENGTH) {
			break;
		} 
		else {
			sample_app_state.sysex_rx_bytes[dest_idx] = data_bytes[i];
		}
	}
#endif
	sample_app_state.sysex_rx_byte_count += num_data_bytes;
}

static void sysex_end_cb(uint8_t cable_num)
{
	if (sample_app_state.sysex_rx_byte_count < CONFIG_SYSEX_ECHO_MAX_LENGTH) {
		sample_app_state.sysex_rx_bytes[sample_app_state.sysex_rx_byte_count] = 0xf7;
	}
	sample_app_state.sysex_rx_byte_count++; // account for the last 0xf7
	u_int64_t dt_ms = k_uptime_get() - sample_app_state.sysex_rx_start_time;
	log_sysex_transfer_time(0, cable_num, sample_app_state.sysex_rx_byte_count, dt_ms);
	flash_rx_led();
#ifdef CONFIG_SYSEX_ECHO_ENABLED
	LOG_INF("Echoing received sysex");
	sysex_tx_will_start(1, sample_app_state.sysex_rx_byte_count < CONFIG_SYSEX_ECHO_MAX_LENGTH ? sample_app_state.sysex_rx_byte_count : CONFIG_SYSEX_ECHO_MAX_LENGTH, cable_num);
	send_next_sysex_chunk();
#endif
}

static void usb_midi_available_cb(int is_available)
{
	sample_app_state.usb_midi_is_available = is_available;
	set_usb_midi_available_led(is_available);
	if (is_available) {
		sample_app_state.tx_note_off = 0;
		sample_app_state.sysex_tx_in_progress = 0;
	}
}

static uint8_t get_next_sysex_tx_byte() {
	if (sample_app_state.sysex_tx_is_echo) {
		return sample_app_state.sysex_rx_bytes[sample_app_state.sysex_tx_byte_count];
	} 
	else {
		if (sample_app_state.sysex_tx_byte_count == 0) {
			return 0xf0;
		}
		else if (sample_app_state.sysex_tx_byte_count == sample_app_state.sysex_tx_msg_size - 1) {
			return 0xf7;
		} 
		else {
			return sample_app_state.sysex_tx_byte_count % 100;
		}	
	}
}

static void send_next_sysex_chunk() {
	__ASSERT_NO_MSG(sample_app_state.sysex_tx_in_progress);
	flash_tx_led();

	while (1) {
		if (usb_midi_tx_buffer_is_full()) {
			// tx packet is full. send it. 
			usb_midi_tx_buffer_send();
			// nothing further for now. wait for tx done callback before
			// filling the next packet.
			break;
		}

		int sysex_msg_size = sample_app_state.sysex_tx_msg_size;

		uint8_t chunk[3] = {0, 0, 0};
		for (int i = 0; i < 3; i++) {
			uint8_t next_sysex_byte = get_next_sysex_tx_byte();
			chunk[i] = next_sysex_byte;
			sample_app_state.sysex_tx_byte_count++;

			if (sample_app_state.sysex_tx_byte_count == sysex_msg_size) {
				break;
			}
		}

		// Enqueue three byte sysex chunk for transmission
		// TODO: check if this suceeds or not? Currently, this check is not needed
		// since each MIDI message is put into a 4 byte packet and the tx buffer size
		// is a multiple of 4. 
		usb_midi_tx_buffer_add(sample_app_state.sysex_tx_cable_num, chunk);

		if (sample_app_state.sysex_tx_byte_count == sysex_msg_size) {
			// No more data to add to tx packet. Send it, then we're done.
			usb_midi_tx_buffer_send();
			flash_tx_led();
			u_int64_t dt_ms = k_uptime_get() - sample_app_state.sysex_tx_start_time;
			log_sysex_transfer_time(1, sample_app_state.sysex_tx_cable_num, sysex_msg_size, dt_ms);
			sample_app_state.sysex_tx_in_progress = 0;
			break;
		}
	}
}

static void usb_midi_tx_done_cb()
{
	if (sample_app_state.sysex_tx_in_progress) {
		send_next_sysex_chunk();
	}
}

/****************** Sample app ******************/
USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

/* doc configuration instantiation start */
static const uint8_t attributes = (IS_ENABLED(CONFIG_SAMPLE_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

/* Full speed configuration */
#define CONFIG_SAMPLE_USBD_MAX_POWER 250
USBD_CONFIGURATION_DEFINE(sample_fs_config,
			  attributes,
			  CONFIG_SAMPLE_USBD_MAX_POWER, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(sample_hs_config,
			  attributes,
			  CONFIG_SAMPLE_USBD_MAX_POWER, &hs_cfg_desc);
/* doc configuration instantiation end */

USBD_DEVICE_DEFINE(usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_SAMPLE_APP_DEVICE_VID, CONFIG_SAMPLE_APP_DEVICE_PID);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, CONFIG_SAMPLE_APP_MANUFACTURER_NAME);
USBD_DESC_PRODUCT_DEFINE(sample_product, CONFIG_SAMPLE_APP_PRODUCT_NAME);
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn); // TODO: where does WHID come from?

int main(void)
{
#if defined(CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT) || NRF_CLOCK_HAS_HFCLK192M
	// Run nrf5340 app core at full speed (128 MHz)
	nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
#endif
	init_leds();
	init_button();

	
	
	int add_config_result = usbd_add_configuration(&usbd, USBD_SPEED_FS,
				     &sample_fs_config);
					 /* doc add string descriptor start */
	printk("usbd_add_configuration result %d\n", add_config_result);
	int register_result =  usbd_register_class(&usbd, "usb_midi", USBD_SPEED_FS, 1);
	printk("usbd_register_class result %d\n", register_result);
	// register_result = usbd_register_class(&usbd, "usb_midi", USBD_SPEED_HS, 1);
	// printk("usbd_register_class result %d\n", register_result);


	int err = usbd_add_descriptor(&usbd, &sample_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
	}

	err = usbd_add_descriptor(&usbd, &sample_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
	}

	err = usbd_add_descriptor(&usbd, &sample_product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
	}

	err = usbd_add_descriptor(&usbd, &sample_sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return NULL;
	}

	int init_result = usbd_init(&usbd);
	printk("usbd_init result %d\n", init_result);
	int enable_result = usbd_enable(&usbd);
	printk("usbd_enable result %d\n", enable_result);

	k_work_init(&button_press_work, on_button_press);
	k_work_init(&event_tx_work, on_event_tx);
	k_work_init_delayable(&rx_led_off_work, on_rx_led_off);
	k_work_init_delayable(&tx_led_off_work, on_tx_led_off);

	/* Register USB MIDI callbacks */
	struct usb_midi_cb_t callbacks = {.available_cb = usb_midi_available_cb,
					  .tx_done_cb = usb_midi_tx_done_cb,
					  .midi_message_cb = midi_message_cb,
					  .sysex_data_cb = sysex_data_cb,
					  .sysex_end_cb = sysex_end_cb,
					  .sysex_start_cb = sysex_start_cb};
	usb_midi_init(&callbacks);

	/* Init USB */
	// int enable_rc = usb_enable(NULL);
	// __ASSERT(enable_rc == 0, "Failed to enable USB");

	/* Send MIDI messages periodically */
	while (1) {
		
#ifdef CONFIG_TX_PERIODIC_NOTE_ENABLED
		k_work_submit(&event_tx_work);
#endif
		k_msleep(CONFIG_TX_PERIODIC_NOTE_INTERVAL_MS);
	}
}