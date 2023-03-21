# zephyr-usb-midi

This is a [USB MIDI 1.0 device class](https://www.usb.org/sites/default/files/midi10.pdf) driver for the [Zephyr RTOS](https://zephyrproject.org/), which allows sending and receiving [MIDI](https://en.wikipedia.org/wiki/MIDI) data (including system exclusive messages) over USB. 

The current implementation, which uses Zephyr's [soon-to-be legacy](https://github.com/zephyrproject-rtos/zephyr/issues/42066) USB stack, is usable but should be considered work in progress and needs more testing before it's ready for real world use. If you run into any issues, please consider [reporting them](https://github.com/stuffmatic/zephyr-usb-midi/issues/new) or [submitting a PR](https://github.com/stuffmatic/zephyr-usb-midi/compare).

## Usage

The USB MIDI device class driver is contained in a Zephyr module. The sample app's [CMakeLists.txt](CMakeLists.txt) file shows one way of adding this module to an app.

The public API is defined in [usb_midi.h](usb_midi/include/usb_midi/usb_midi.h).

## Sample app

The [sample app](src/main.c) shows how to send and receive MIDI data. Note on/off messages are sent periodically when connected to a host. The app should work on dev boards with at least one button and three LEDs, for example [stm32f4_disco](https://docs.zephyrproject.org/latest/boards/arm/stm32f4_disco/doc/index.html) and [nrf52840dk_nrf52840](https://docs.zephyrproject.org/latest/boards/arm/nrf52840dk_nrf52840/doc/index.html).

* __Button 1__ - Send sysex message
* __LED 1__ - On when the device is connected to a host
* __LED 2__ - Flashes when MIDI data is received
* __LED 3__ - Flashes when MIDI data is sent 

https://user-images.githubusercontent.com/2444852/226658203-de83b3d5-6604-40a9-8dde-cb53ff2cb486.mp4

## Configuration options

* `CONFIG_USB_DEVICE_MIDI`- Set to `y` to enable the USB MIDI device class driver.
* `CONFIG_USB_MIDI_NUM_INPUTS` - The number of jacks through which MIDI data flows into the device. Between 0 and 16 (inclusive). Defaults to 1.
* `CONFIG_USB_MIDI_NUM_OUTPUTS` - The number of jacks through which MIDI data flows out of the device. Between 0 and 16 (inclusive). Defaults to 1.
* `CONFIG_USB_MIDI_USE_CUSTOM_JACK_NAMES` - Set to `y` to use custom input and output jack names defined by the options below.
* `CONFIG_USB_MIDI_INPUT_JACK_n_NAME` - the name of input jack `n`, where `n` is the cable number of the jack.
* `CONFIG_USB_MIDI_OUTPUT_JACK_n_NAME` - the name of output jack `n`, where `n` is the cable number of the jack.
