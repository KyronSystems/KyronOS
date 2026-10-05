# Building

For Windows development without WSL2, see [WINDOWS.md](WINDOWS.md).

1. Run `./setup.sh`.
2. Install the printed prerequisites if checks fail. On Fedora, use `sudo dnf install gcc gcc-c++ make nasm grub2-tools-extra grub2-efi-x64-modules mtools xorriso gdb`. On Debian/Ubuntu, use `sudo apt install build-essential nasm grub-pc-bin grub-efi-amd64-bin grub-common mtools xorriso gdb`. Install VirtualBox separately.
3. Run `make kernel`.
4. Run `make iso` to create `build/kyronos.iso`.
5. Run `make run` to boot it in VirtualBox, or `make run EFI=on` to boot it with VirtualBox EFI firmware.
6. Run `make installer-iso` to build CD media with a **Install KyronOS** GRUB entry and deployable hybrid boot image.
7. Boot that entry with a target disk attached, then run `install` followed by `install confirm` in the shell.
8. Run `make disk-image` to create a 256 MiB preformatted hybrid hard-drive image, then `make run-disk` or `make run-disk EFI=on` to boot it.
9. Run `make update-iso` to create update media. Boot its **Update installed KyronOS** GRUB entry, then type `update` and `update confirm`.
10. Run `make debug` to prepare the image for a separately configured VirtualBox debug session.

The build uses GRUB Multiboot2 and NASM. Fedora names the rescue command `grub2-mkrescue`; the Makefile detects both Fedora and Debian/Ubuntu names. A host compiler may produce the bootstrap ELF, but a freestanding cross compiler should be used for future kernel expansion. The setup script never installs packages automatically.

The ISO includes `bg.jpg` as the desktop wallpaper. GRUB's `jpeg` and `gfxterm` modules draw it before handing the requested 1024x768x32 framebuffer to the kernel. Replace `bg.jpg` before running `make iso` to use another JPEG wallpaper. The hybrid ISO carries both BIOS and x86-64 UEFI boot entries. The bootable disk image reserves blocks before the KSFS slice. The installer writes the GRUB hybrid image into the boot area, then formats the KSFS slice separately.

The update-media menu passes a padded 512 KiB kernel slot as a Multiboot2 module. `update confirm` locates `/boot/kyronos.kernel` in the installed hybrid ISO9660 boot area and replaces only that extent. Keep new kernels below 512 KiB. This updater requires a bootable KyronOS hybrid image on an attached IDE/AHCI disk.
