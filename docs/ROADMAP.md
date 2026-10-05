# Roadmap

## Alpha 2

- [x] Project structure and build pipeline
- [x] Multiboot2 kernel bootstrap
- [x] Text console identity
- [x] KSFS superblock format and validation core
- [x] Generic block-device interface
- [x] PCI configuration-space scanning and IDE/ATA PIO block-device prototype
- [x] SATA AHCI DMA block-device support
- [ ] x86-64 long-mode entry and memory map parsing
- [x] PS/2 keyboard and interactive shell
- [x] KSFS inode, directory, and file operations on persistent storage
- [x] KSFS persistent shell mount and confirmation-gated install flow
- [x] Installer CD deploys the hybrid boot image and formats the separate KSFS volume
- [x] Hybrid GRUB BIOS/UEFI disk image and reserved data volume
- [x] Translucent framebuffer desktop, dock, and Notes/Files/Settings/Images apps
- [x] BMP decoding and viewing
- [x] CD-loaded update module for the embedded boot-kernel slot
- [x] PS/2 wheel and five-button mouse support
- [ ] USB EHCI/xHCI host controller and HID report transport
- [ ] General partition-table discovery and arbitrary physical-disk bootloader installation
- [ ] ATAPI optical-drive transport and direct CD filesystem access
- [x] Offset-aware `ksfs-mkfs` image tool

Later releases can add broader partition formats, native USB host controllers, optical-drive transport, and a 64-bit kernel. Unsupported behavior must remain reported as unsupported until implemented.
