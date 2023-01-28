* zephyr unit tests
* what device descr fields (etc) gets overridden by zephyr?
* logging
* connect/reconnect sometimes gives USB_DC_RESET
* notification of data sent
* device .bMaxPacketSize0 8 as in the spec example gives [00:00:00.557,189] <wrn> usb_device: Failed to write endpoint buffer 0x80. and the device does not enumerate .bMaxPacketSize0 64 seems to work fine (macOS)
*  <wrn> usb_device: Endpoint 0x01 already enabled, <wrn> usb_device: Endpoint 0x81 already enabled on reconnecting
* https://github.com/kcuzner/midi-fader/blob/master/firmware/src/usb_midi.c
* rely on zephyr device stack to populate device descriptor?
* validate no interleaved system realtime bytes

Inspiration:

* https://conductivelabs.com/forum/showthread.php?tid=1227
* https://github.com/kcuzner/midi-fader
* https://www.usb.org/sites/default/files/USB%20MIDI%20v2_0.pdf
* https://github.com/BlokasLabs/USBMIDI/
* https://github.com/btrepp/usbd-midi/blob/master/src/midi_device.rs
* https://warrantyvoids.github.io/ModSynth/modules/midi2cv/midi-descriptor
* https://www.microchip.com/forums/m493322.aspx
* https://github.com/lathoub/Arduino-USBMIDI
* https://github.com/arduino-libraries/MIDIUSB/blob/master/src/MIDIUSB.cpp
* https://www.latke.net/asp_digital/?p=61
* http://janaxelson.com/forum/index.php?topic=1862.0
* https://github.com/hathach/tinyusb/blob/master/src/device/usbd.h
* https://www.latke.net/asp_digital/?p=61

