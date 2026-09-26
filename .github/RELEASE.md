# KyronOS Alpha 2

KyronOS Alpha 2 is a text-only live operating system for x86 PCs. It boots from a GRUB Multiboot2 ISO and provides a VGA console, PS/2 keyboard input, an interactive shell, directory navigation, users, and a RAM-backed filesystem.

## Highlights

- RAM-only live shell with no persistent command history.
- Improved `ls` handling for subdirectories.
- Nested tab completion with `/` appended to completed directories.
- Default user starts in `/home/kyron`.
- Prompt format: `[user@hostname]{directory}#`.
- Formatted command help and `[command] help` support.
- Alpha 2 branding throughout the live OS.
- Seeded root layout with editable command stubs under `/usr/bin` and source/config notes under `/system` and `/etc`.
- PCI configuration-space scanning.
- IDE/ATA PIO block-device prototype for future storage work.
- BIOS and preliminary UEFI boot entries in the generated ISO.
- Windows development documentation for WSL2 and VirtualBox workflows.

## Build

Build from Ubuntu WSL2:

```sh
cd /mnt/c/Users/<WindowsUser>/OneDrive/Documents/KyronOS
make clean
make kernel
make test
make iso
```

The generated live image is `build/kyronos.iso`.

## Boot status

BIOS boot is the primary supported path for Alpha 2. The ISO also contains a GRUB UEFI El Torito entry and can be selected in VirtualBox with EFI enabled, but native EFI kernel services and a native EFI kernel entry are not complete or fully validated.

## Known limitations

- The live OS is RAM-only; files and shell changes disappear on reboot.
- There is no disk installer or live-only `setup` command.
- SATA/AHCI, NVMe, SCSI, eMMC, SD, and USB mass-storage drivers are not implemented.
- Partition discovery and installed-disk mounting are not implemented.
- The IDE/ATA PIO prototype is not yet connected to the live shell.
- The kernel remains a 32-bit Multiboot2 bootstrap.
- EFI support is preliminary and should be treated as experimental.
