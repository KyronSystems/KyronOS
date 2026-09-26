# Roadmap

## Alpha 2

- [x] Project structure and build pipeline
- [x] Multiboot2 kernel bootstrap
- [x] Text console identity
- [x] KSFS superblock format and validation core
- [x] Generic block-device interface
- [x] PCI configuration-space scanning and IDE/ATA PIO block-device prototype
- [ ] x86-64 long-mode entry and memory map parsing
- [x] PS/2 keyboard and interactive shell
- [x] KSFS inode, directory, and file operations on persistent storage
- [ ] Storage controller and partition support for IDE, SATA, SCSI, NVMe, eMMC, USB, and SD devices
- [ ] Installer with live-image-only setup command
- [ ] EFI-native boot path
- [ ] `ksfs-mkfs` image tool
- [ ] USB EHCI/xHCI detection
- [ ] CD/DVD read support
- [ ] Full command set and `notepad`
- [ ] Hard-drive drivers
- [ ] GUI or desktop environment

Later releases can add networking, richer userland, storage drivers, and eventually a graphical subsystem. Unsupported behavior must remain reported as unsupported until implemented.
