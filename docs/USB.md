# USB keyboard support

The USB keyboard input boundary is `usb::keyboard::decode_boot_report()`. It accepts an 8-byte USB HID boot-protocol keyboard report and translates standard keyboard usages, Shift, and punctuation into shell characters.

`usb::mouse::decode_boot_report()` accepts USB HID boot-mouse reports with buttons, signed X/Y movement, and optional wheel data. `mouse::consume_usb_report()` routes a decoded report into the same pointer path as PS/2.

Native USB hardware support still requires an EHCI/xHCI controller driver to discover interfaces and deliver interrupt-in transfers to these decoders. Until that transport exists, USB devices may work only through firmware legacy PS/2 emulation. Report decoding does not claim controller support by itself.
