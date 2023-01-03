# zephyr-usb-midi

This repo contains a [Zephyr](https://zephyrproject.org/) implementation of the [USB MIDI device class](https://www.usb.org/sites/default/files/midi10.pdf) along with a [small test app](src/main.c). As the name suggests, this class lets a device running Zephyr send and receive [MIDI](https://en.wikipedia.org/wiki/MIDI) data over USB following a widely supported standard.

__The code is still work in progress__, but the goal is to get it in good enough shape to submit a PR to mainline Zephyr. Any help to achieve this goal would be greatly appreciated, for example

* testing on different boards
* testing compatibility with different host operating systems
* finding, [reporting](https://github.com/stuffmatic/zephyr-usb-midi/issues) and fixing bugs
* suggesting or implementing improvements
* testing with sysex data

## What works and what doesn't (yet)

It's currently possible to create a USB MIDI device with a configurable number of input and output ports and have the device enumerated by the host (tested on macOS). Sending and receiving data works, but there are still some issues to sort out related to the encoding of outgoing MIDI data. Here's what a connected device with 3 inputs and 2 outputs looks like on macOS:

<img src="midi_studio.png" width="500">

For known issues and missing features, see the [issue tracker](https://github.com/stuffmatic/zephyr-usb-midi/issues).

## Usage

See the public API defined in [usb_midi.h](src/usb_midi.h).

## Configuration options

* `CONFIG_USB_MIDI_NUM_INPUTS` - The number of ports through which MIDI data flows into the device. Between 0 and 16 (inclusive). Defaults to 1.
* `CONFIG_USB_MIDI_NUM_OUTPUTS` - The number of ports through which MIDI data flows out of the device. Between 0 and 16 (inclusive). Defaults to 1.

## Development

Development work so far has been done on macOS using [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK) v2.1.2 and an [nRF52840 DK board](https://www.nordicsemi.com/Products/Development-hardware/nRF52840-DK). 

[MIDI Monitor](https://www.snoize.com/midimonitor/) and [MidiKeys](https://flit.github.io/projects/midikeys/) are open source macOS apps that have been useful during development.

On macOS, the MIDI Studio window of the built in Audio MIDI Setup app is useful for inspecting a connected device and making sure it is properly enumrated. ⚠️ __Warning:__ MIDI Studio seems to cache device info between connections, which means changes to for example device name and port configuration won't show up unless you disconnect the device, remove the dimmed device box and reconnect the device.