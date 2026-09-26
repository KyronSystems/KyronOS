# KyronOS

KyronOS is a small, text-only experimental operating system for x86 PCs. It boots from a GRUB Multiboot2 ISO and provides a VGA terminal with a shell, keyboard input, directory navigation, users, and a RAM-backed filesystem prototype.

## Real hardware

Video of KyronOS running on real hardware:

[Watch `real_hardware_2000.mp4`](.github/assets/real_hardware_2000.mp4)

<video src=".github/assets/real_hardware_2000.mp4" controls></video>

## Build

Verify the host tools without installing anything:

```sh
./setup.sh
```

On Fedora:

```sh
sudo dnf install gcc gcc-c++ make nasm grub2-tools-extra xorriso gdb
```

On Debian or Ubuntu:

```sh
sudo apt install build-essential nasm grub-pc-bin grub-common xorriso gdb
```

Build and run the ISO:

```sh
make iso
make run
```

The generated image is `build/kyronos.iso`. VirtualBox can boot it with `make run`; use `make run EFI=on` for an EFI VM. The `debug` target only prepares the image because VirtualBox debugging requires separate VM debug configuration.

```sh
gdb build/kyronos.kernel
```

## Shell

The current shell includes:

```text
help clear version about echo
ls cd pwd go back go home go root
touch mkdir rmdir write append cat rm
add user <name>
switch user <name>
devices mem reboot shutdown
```

The default root layout is:

```text
/
├── kyron/
├── boot/
├── system/
├── etc/
├── home/
├── tmp/
├── dev/
├── bin/
└── usr/
	└── bin/
```

User home directories live under `/home`. Inside the active user's home, the prompt uses `~`, for example `[kyron]:~#` and `[kyron]:~/projects#`.

## Project status

Implemented foundations include the Multiboot2 boot image, VGA console with scrolling and cursor tracking, PS/2 keyboard input with Shift and Tab completion, shell navigation, user home-directory handling, KSFS superblock validation, and a generic block-device interface.

KyronOS does not yet provide native storage-controller transport, partition discovery, persistent disk installation, or a complete on-disk KSFS file tree. USB boot-protocol decoding is present, but native USB keyboard support requires the controller and interrupt-transfer layers. The current GRUB image can be selected in VirtualBox with BIOS or EFI firmware; the installer and persistent disk path remain future work.

See [docs/BUILDING.md](docs/BUILDING.md), [docs/WINDOWS.md](docs/WINDOWS.md), [docs/KSFS.md](docs/KSFS.md), [docs/USB.md](docs/USB.md), and [docs/ROADMAP.md](docs/ROADMAP.md) for details.
