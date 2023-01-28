# zephyr-usb-midi

This repo contains a [Zephyr](https://zephyrproject.org/) implementation of the [USB MIDI 1.0 device class](https://www.usb.org/sites/default/files/midi10.pdf) along with a [small test app](src/main.c). As the name suggests, this class lets a device running Zephyr send and receive [MIDI](https://en.wikipedia.org/wiki/MIDI) data over USB using a widely supported standard protocol.

TODO: The code for parsing and serializing USB MIDI packets is completely self contained and does not have any external dependencies. This makes it both easy to test and use in other projects.

## Usage

The public API is defined in [usb_midi.h](src/usb_midi/usb_midi.h).

To try out the code in your own app, grab the code in [`src/usb_midi`](src/usb_midi), make sure you declare the config vars from [`Kconfig`](Kconfig) and set `CONFIG_USB_DEVICE_STACK=y` in your `.conf` file.

## Configuration options

* `CONFIG_USB_MIDI_NUM_INPUTS` - The number of jacks through which MIDI data flows into the device. Between 0 and 16 (inclusive). Defaults to 1.
* `CONFIG_USB_MIDI_NUM_OUTPUTS` - The number of jacks through which MIDI data flows out of the device. Between 0 and 16 (inclusive). Defaults to 1.
* `CONFIG_USB_MIDI_USE_CUSTOM_JACK_NAMES` - Set to `y` to use custom input and output jack names defined by the options below.
* `CONFIG_USB_MIDI_INPUT_JACK_n_NAME` - the name of input jack `n`, where `n` is the cable number of the jack.
* `CONFIG_USB_MIDI_OUTPUT_JACK_n_NAME` - the name of output jack `n`, where `n` is the cable number of the jack.

## Custom jack names

### macOS

The images below show how custom jack names appear in [MidiKeys](https://flit.github.io/projects/midikeys/) on macOS.

<img src="images/macos_input_names.png" width="300"> <img src="images/macos_output_names.png" width="300">

By using the same name for both jacks in an input/ouput pair, MIDI Studio on macOS will display them as a port with that name. For example, these jack names

```
CONFIG_USB_MIDI_INPUT_JACK_0_NAME="Custom port name 1"
CONFIG_USB_MIDI_OUTPUT_JACK_0_NAME="Custom port name 1"
CONFIG_USB_MIDI_INPUT_JACK_1_NAME="Custom port name 2"
CONFIG_USB_MIDI_OUTPUT_JACK_1_NAME="Custom port name 2"
```

result in these port names

<img src="images/macos_port_names.png" width="500">

## USB MIDI device topology

The [USB MIDI 1.0 spec](https://www.usb.org/sites/default/files/midi10.pdf) allows for a wide range of device topologies, i.e ways to interconnect a number of enteties to form a description of a USB MIDI device. Ideally, this description should match the functionality of the physical device.

The spec is thin on topology examples and only provides one for a MIDI adapter (see appendix B) with external input and output jacks that are connected to embedded ouput and input jacks respectively. This topology seems fairly common in open source implementations, but does not seem ideal for devices without without physical MIDI jacks. This implementation uses embedded input and output jacks connected to an element entity corresponding to the device, assuming the most common device type is one without physical jacks.

It is not entirely clear to me how the device topology (beyond the embedded jacks) is reflected in various hosts. If you have any examples, I'd be interested to know. Feel free to [post them in an issue](https://github.com/stuffmatic/zephyr-usb-midi/issues/new).

## Development notes

Development work so far has been done on macOS using [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK) v2.1.2 and an [nRF52840 DK board](https://www.nordicsemi.com/Products/Development-hardware/nRF52840-DK).

[MIDI Monitor](https://www.snoize.com/midimonitor/) and [MidiKeys](https://flit.github.io/projects/midikeys/) are open source macOS apps that have been useful during development.

[USB in a nutshell](https://beyondlogic.org/usbnutshell/usb1.shtml)

On macOS, the MIDI Studio window of the built in Audio MIDI Setup app is useful for inspecting a connected device and making sure it is properly enumrated. ⚠️ __Warning:__ MIDI Studio seems to cache device info between connections, which means changes to device name, port configuration etc won't show up unless you disconnect the device, remove the dimmed device box and reconnect the device.