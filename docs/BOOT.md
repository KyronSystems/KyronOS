# Boot

The ISO uses GRUB's Multiboot2 protocol. `boot/entry.asm` validates the entry ABI enough to pass the Multiboot magic and information pointer to `kmain`. The linker keeps the Multiboot header at the beginning of the loaded image. The image can be tested in VirtualBox with either BIOS firmware or its EFI firmware setting. Native EFI services, page tables, a GDT, and a true x86-64 kernel entry point are still future boot milestones.
