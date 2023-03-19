# zephyr-usb-midi

This is a [USB MIDI 1.0 device class](https://www.usb.org/sites/default/files/midi10.pdf) driver for [Zephyr](https://zephyrproject.org/), which allows sending and receiving [MIDI](https://en.wikipedia.org/wiki/MIDI) data over USB.

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

## Sample application

A sample application can be found in [src/main.c](src/main.c). 
3 outputs, 2 inputs.

### LEDs

* __LED 1__ - Turns on when BLE MIDI is available.
* __LED 2__ - Toggles on/off when receiving sysex messages.
* __LED 3__ - Toggles on/off when receiving non-sysex messages with cable number 0.
* __LED 4__- Toggles on/off when receiving non-sysex messages with cable number 1.

### Buttons

* __Button 1__ - Send note on/off on cable number 0.
* __Button 2__ - Send note on/off on cable number 1.
* __Button 3__ - Send note on/off on cable number 2.
* __Button 4__ - Send a streaming sysex message on cable number 0.

## Development notes

### Packet encoding tests

To facilitate testing and reuse in other projects, the code for parsing, encoding and decoding USB MIDI packets is completely self contained and has no external dependencies.

### Useful apps

open source macOS apps have been useful during development:

* [MIDI Monitor](https://www.snoize.com/midimonitor/)  
* [MidiKeys](https://flit.github.io/projects/midikeys/)
* [SysEx Librarian](https://www.snoize.com/sysexlibrarian/) 

[USB in a nutshell](https://beyondlogic.org/usbnutshell/usb1.shtml)

On macOS, the MIDI Studio window of the built in Audio MIDI Setup app is useful for inspecting a connected device and making sure it is properly enumrated. ⚠️ __Warning:__ MIDI Studio seems to cache device info between connections, which means changes to device name, port configuration etc won't show up unless you disconnect the device, remove the dimmed device box and reconnect the device.

