# zephyr-usb-midi

This is a [USB MIDI 1.0 device class](https://www.usb.org/sites/default/files/midi10.pdf) driver for [Zephyr](https://zephyrproject.org/), which allows sending and receiving [MIDI](https://en.wikipedia.org/wiki/MIDI) data over USB. The current implementation uses Zephyr's [soon-to-be legacy USB stack](https://github.com/zephyrproject-rtos/zephyr/issues/42066).

## Usage

The USB MIDI device class driver is contained in a Zephyr module. In addition to this module, this repo also contains a [sample app](src/main.c) showing how to send and receive MIDI data. The sample app adds the driver module in its [CMakeLists.txt](CMakeLists.txt) file.

The public API is defined in [usb_midi.h](usb_midi/include/usb_midi/usb_midi.h).

## Configuration options

* `CONFIG_USB_DEVICE_MIDI`- Set to `y` to enable the USB MIDI device class driver.
* `CONFIG_USB_MIDI_NUM_INPUTS` - The number of jacks through which MIDI data flows into the device. Between 0 and 16 (inclusive). Defaults to 1.
* `CONFIG_USB_MIDI_NUM_OUTPUTS` - The number of jacks through which MIDI data flows out of the device. Between 0 and 16 (inclusive). Defaults to 1.
* `CONFIG_USB_MIDI_USE_CUSTOM_JACK_NAMES` - Set to `y` to use custom input and output jack names defined by the options below.
* `CONFIG_USB_MIDI_INPUT_JACK_n_NAME` - the name of input jack `n`, where `n` is the cable number of the jack.
* `CONFIG_USB_MIDI_OUTPUT_JACK_n_NAME` - the name of output jack `n`, where `n` is the cable number of the jack.