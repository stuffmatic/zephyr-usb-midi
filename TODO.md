* validate no interleaved system realtime bytes
* proper zephyr code formating
* select instead of depends on for kconfig vars?
* macos resume after sleep sometimes not working (only when logging?)
* wMaxPacketSize=64 or 512 depending on hs/fs, 
* hs config only?

# USB next port

macbook goes to sleep, then:

[00:03:25.961,425] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 4
[00:03:25.961,578] <dbg> usb_midi: usb_midi_request_cb: 0x20000898 -> ep 0x81, len 4, err 0
[00:03:26.461,395] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 4
[00:03:26.962,371] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 8
[00:03:27.462,371] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 12
[00:03:27.962,341] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 16
[00:03:28.462,341] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 20
[00:03:28.962,341] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 24
[00:03:29.463,287] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 28
[00:03:29.963,256] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 32
[00:03:30.463,256] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 36
[00:03:30.963,226] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 40
[00:03:31.464,202] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 44
[00:03:31.964,202] <dbg> udc: udc_ep_enqueue: Queue ep 0x81 0x200246ac len 48
[00:03:32.464,202] <dbg> udc: udc_ep_x00000004  r1/a2:  0x0000003e  r2/a3:  0x00000003
[00:03:41.218,719] <err> os: r3/a4:  0x00000004 r12/ip:  0x00000004 r14/lr:  0x00006467
[00:03:41.218,750] <err> os:  xpsr:  0x01000000
[00:03:41.218,750] <err> os: Faulting instruction address (r15/pc): 0x00015a80
[00:03:41.218,811] <err> os: >>> ZEPHYR FATAL ERROR 4: Kernel panic on CPU 0
[00:03:41.218,841] <err> os: Current thread: 0x20020cc8 (usbd)
[00:03:41.293,975] <err> os: Halting system