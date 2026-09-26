# Building

For Windows development without WSL2, see [WINDOWS.md](WINDOWS.md).

1. Run `./setup.sh`.
2. Install the printed prerequisites if checks fail. On Fedora, use `sudo dnf install gcc gcc-c++ make nasm grub2-tools-extra grub2-efi-x64-modules mtools xorriso gdb`. On Debian/Ubuntu, use `sudo apt install build-essential nasm grub-pc-bin grub-efi-amd64-bin grub-common mtools xorriso gdb`. Install VirtualBox separately.
3. Run `make kernel`.
4. Run `make iso` to create `build/kyronos.iso`.
5. Run `make run` to boot it in VirtualBox, or `make run EFI=on` to boot it with VirtualBox EFI firmware.
6. Run `make debug` to prepare the image for a separately configured VirtualBox debug session.

The build uses GRUB Multiboot2 and NASM. Fedora names the rescue command `grub2-mkrescue`; the Makefile detects both Fedora and Debian/Ubuntu names. A host compiler may produce the bootstrap ELF, but a freestanding cross compiler should be used for future kernel expansion. The setup script never installs packages automatically.

The ISO includes `bg.jpg` as the desktop wallpaper. GRUB's `jpeg` and `gfxterm` modules draw it before handing the 32-bit framebuffer to the kernel; replace `bg.jpg` before running `make iso` to use another JPEG wallpaper. The kernel displays its fixed Terminal window over the preserved framebuffer and accepts PS/2 mouse input.
