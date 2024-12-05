# SPDX-License-Identifier: Apache-2.0

if(CONFIG_USB_DEVICE_MIDI)
  zephyr_include_directories(./include)

  zephyr_library()
  zephyr_library_sources(./src/usb_midi_packet.c)
  zephyr_library_sources_ifdef(CONFIG_USB_DEVICE_STACK_NEXT ./src/usb_midi.c)
  zephyr_library_sources_ifdef(CONFIG_USB_DEVICE_STACK ./src/usb_midi_legacy.c)
endif()
