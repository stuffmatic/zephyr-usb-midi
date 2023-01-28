#include <stdio.h>
#include "../src/usb_midi/usb_midi_packet.h"

static void assert(int condition, const char* msg) {
    if (!condition) {
        printf("❌ Assertion failed: %s\n", msg);
    }
}

static void reset_packet(struct usb_midi_packet_t *packet)
{
	packet->bytes[0] = 0;
	packet->bytes[1] = 0;
	packet->bytes[2] = 0;
	packet->bytes[3] = 0;

	packet->cin = 0;
	packet->num_midi_bytes = 0;
	packet->cable_num = 0;
}

static void test_packet_from_midi_bytes()
{
	struct usb_midi_packet_t packet;
	uint8_t cable_num = 7;

	/* Test building packets from channel messages */
	for (uint8_t high_nibble = 0x8; high_nibble < 0xf; high_nibble++)
	{
		uint8_t msg[3] = {
			high_nibble << 4, 0x12, 0x23
		};
		enum usb_midi_error_t result = usb_midi_packet_from_midi_bytes(msg, cable_num, &packet);
		assert(result == USB_MIDI_SUCCESS, "Parsing valid channel message should succeed");
		assert(packet.cin == high_nibble, "Channel message CIN should equal high status byte nibble");
		assert(packet.cable_num == cable_num, "Unexpected USB MIDI packet cable number");
		if (high_nibble == 0xc || high_nibble == 0xd) {
				assert(packet.num_midi_bytes == 2, "Unexpected MIDI byte count");
		} else {
				assert(packet.num_midi_bytes == 3, "Unexpected MIDI byte count");
		}
	}

    /* Test building packets from sysex, system realtime and system common messages */
    uint8_t messages[][4] = {
        /* System real time */
        { 0xf8, 0, 0, USB_MIDI_CIN_1BYTE_DATA }, /* Timing Clock */
        { 0xfa, 0, 0, USB_MIDI_CIN_1BYTE_DATA }, /* Start */
        { 0xfb, 0, 0, USB_MIDI_CIN_1BYTE_DATA }, /* Continue */
        { 0xfc, 0, 0, USB_MIDI_CIN_1BYTE_DATA }, /* Stop */
        { 0xfe, 0, 0, USB_MIDI_CIN_1BYTE_DATA }, /* Active Sensing */
        { 0xff, 0, 0, USB_MIDI_CIN_1BYTE_DATA }, /* System Reset */
        /* System common */
        { 0xf1, 0x1, 0, USB_MIDI_CIN_SYSCOM_2BYTE }, /* MIDI Time Code Quarter Frame */
        { 0xf2, 0x2, 0x3, USB_MIDI_CIN_SYSCOM_3BYTE }, /* Song Position Pointer */
        { 0xf3, 0x4, 0, USB_MIDI_CIN_SYSCOM_2BYTE }, /* Song Select */
        { 0xf6, 0, 0 , USB_MIDI_CIN_SYS_COMMON_OR_SYSEX_END_1BYTE }, /* Tune request */
        /* Sysex */
        { 0xf0, 0xf7, 0, USB_MIDI_CIN_SYSEX_END_2BYTE },
        { 0xf0, 0x11, 0xf7, USB_MIDI_CIN_SYSEX_END_3BYTE },
        { 0xf0, 0x11, 0x11, USB_MIDI_CIN_SYSEX_START_OR_CONTINUE },
        { 0x11, 0x11, 0x11, USB_MIDI_CIN_SYSEX_START_OR_CONTINUE },
        { 0x11, 0x11, 0xf7, USB_MIDI_CIN_SYSEX_END_3BYTE },
        { 0x11, 0xf7, 0x0, USB_MIDI_CIN_SYSEX_END_2BYTE },
        { 0xf7, 0x0, 0x0, USB_MIDI_CIN_SYS_COMMON_OR_SYSEX_END_1BYTE }
    };

    uint8_t message_count = sizeof(messages) / 4;
    for (int i = 0; i < message_count; i++) {
		enum usb_midi_error_t result = usb_midi_packet_from_midi_bytes(messages[i], cable_num, &packet);
		assert(result == USB_MIDI_SUCCESS, "Parsing valid channel message should succeed");
		assert(packet.cin == messages[i][3], "Channel message CIN should equal high status byte nibble");
		assert(packet.cable_num == cable_num, "Unexpected USB MIDI packet cable number");
    }

    /* Test invalid status bytes */
    uint8_t invalid_messages[][3] = {
        {0xf4, 0, 0},
        {0xf5, 0, 0},
        {0xf9, 0, 0},
        {0xfd, 0, 0}
    };
    uint8_t invalid_message_count = sizeof(invalid_messages) / 3;
    for (int i = 0; i < invalid_message_count; i++) {
        enum usb_midi_error_t result = usb_midi_packet_from_midi_bytes(invalid_messages[i], cable_num, &packet);
	    assert(result == USB_MIDI_ERROR_INVALID_MIDI_MSG, "Parsing valid channel message should succeed");
    }
}

struct parser_test_result_t {
    uint8_t num_non_sysex_messages;
    uint8_t non_sysex_messages[256][3];
    uint8_t sysex_write_pos;
    uint8_t sysex_messages[256];
};

static struct parser_test_result_t parser_test_result;
static void reset_parser_test_state() {
    parser_test_result.num_non_sysex_messages = 0;
    parser_test_result.sysex_write_pos = 0;
}

static void usb_midi_message_cb(uint8_t *bytes, uint8_t num_bytes, uint8_t cable_num)
{
    uint8_t* msg = parser_test_result.non_sysex_messages[parser_test_result.num_non_sysex_messages];
    for (int i = 0; i < 3; i++) {
        msg[i] = i < num_bytes ? bytes[i] : 0;
    }
    parser_test_result.num_non_sysex_messages++;
}

static void usb_midi_sysex_start_cb(uint8_t cable_num)
{
    parser_test_result.sysex_messages[parser_test_result.sysex_write_pos] = 0xf0;
    parser_test_result.sysex_write_pos++;
}

static void usb_midi_sysex_data_cb(uint8_t* data_bytes, uint8_t num_data_bytes, uint8_t cable_num)
{
    for (int i = 0; i < num_data_bytes; i++) {
        parser_test_result.sysex_messages[parser_test_result.sysex_write_pos] = data_bytes[i];
        parser_test_result.sysex_write_pos++;
    }
}

static void usb_midi_sysex_end_cb(uint8_t cable_num)
{
    parser_test_result.sysex_messages[parser_test_result.sysex_write_pos] = 0xf7;
    parser_test_result.sysex_write_pos++;
}

static struct usb_midi_parse_cb_t parse_cb = {
        .message_cb = usb_midi_message_cb,
        .sysex_start_cb = usb_midi_sysex_start_cb,
        .sysex_data_cb = usb_midi_sysex_data_cb,
        .sysex_end_cb = usb_midi_sysex_end_cb,
    };

struct parser_test_case_t {
    uint8_t msg_bytes[3];
};

static void test_parse_non_sysex() {
    uint8_t messages[][3] = {
        /* Channel messages */
        { 0x81, 0x01, 0x02 }, // note off
        { 0x91, 0x03, 0x04 }, // note on
        { 0xa1, 0x05, 0x06 }, // Polyphonic aftertouch
        { 0xb1, 0x07, 0x08 }, // Control change
	    { 0xc1, 0x09, 0x00 }, // Program change
	    { 0xd1, 0xa, 0x00 }, // Channel aftertouch
	    { 0xe1, 0x0b, 0x0c }  // Pitch bend change
    };
    uint8_t num_messages = sizeof(messages) / 3;
    uint8_t cable_num = 13;

    for (int i = 0; i < num_messages; i++) {
        struct usb_midi_packet_t packet;
        enum usb_midi_error_t error = usb_midi_packet_from_midi_bytes(messages[i], cable_num, &packet);
        reset_parser_test_state();
        usb_midi_parse_packet(packet.bytes, &parse_cb);
        assert(error == USB_MIDI_SUCCESS, "usb_midi_packet_from_midi_bytes should not fail for valid non-sysex msg");
         for (int j = 0; j < packet.num_midi_bytes; j++) {
            assert(parser_test_result.non_sysex_messages[0][j] == messages[i][j], "");
        }
    }
}

static void test_parse_sysex() {
    uint8_t cable_num = 11;
    uint8_t sysex_messages[][3] = {
        { 0xf0, 0xf7, 0 },
        { 0xf0, 0x11, 0xf7 },
        { 0xf0, 0x12, 0x13 },
        { 0x14, 0x15, 0x16 },
        { 0x17, 0x18, 0xf7 },
        { 0x19, 0xf7, 0 },
        { 0xf7, 0, 0 }
    };
    uint8_t num_sysex_messages = sizeof(sysex_messages) / 3;

    for (int i = 0; i < num_sysex_messages; i++) {
        struct usb_midi_packet_t packet;
        enum usb_midi_error_t error = usb_midi_packet_from_midi_bytes(sysex_messages[i], cable_num, &packet);
        assert(error == USB_MIDI_SUCCESS, "usb_midi_packet_from_midi_bytes should not fail for valid sysex msg");
        reset_parser_test_state();
        usb_midi_parse_packet(packet.bytes, &parse_cb);
        assert(parser_test_result.num_non_sysex_messages == 0, "");
        assert(parser_test_result.sysex_write_pos == packet.num_midi_bytes, "");
        for (int j = 0; j < packet.num_midi_bytes; j++) {
            assert(parser_test_result.sysex_messages[j] == sysex_messages[i][j], "");
        }
    }
}

int main(int argc, char *argv[])
{
    test_packet_from_midi_bytes();
    test_parse_sysex();
    test_parse_non_sysex();
}