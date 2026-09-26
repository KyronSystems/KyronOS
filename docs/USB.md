# USB keyboard support

The USB keyboard input boundary is `usb::keyboard::decode_boot_report()`. It accepts an 8-byte USB HID boot-protocol keyboard report and translates standard keyboard usages, Shift, and punctuation into shell characters.

Native hardware support still requires an EHCI/xHCI controller driver to discover interfaces and deliver interrupt-in transfers to this decoder. Until that transport exists, USB keyboards may work only through firmware legacy PS/2 emulation. The decoder does not claim controller support by itself.
