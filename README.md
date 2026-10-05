# KyronOS

KyronOS is an experimental 32-bit x86 operating system with a GRUB Multiboot2 boot path, translucent framebuffer desktop, PS/2 input, IDE/AHCI disk drivers, and KSFS persistent storage.

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

Build the live/update ISO:

```sh
make iso
make run
```

The generated image is `build/kyronos.iso`. VirtualBox can boot it with `make run`; use `make run EFI=on` for an EFI VM. The `debug` target only prepares the image because VirtualBox debugging requires separate VM debug configuration.

Build and boot a hybrid BIOS/EFI hard-drive image:

```sh
make disk-image
make run-disk
```

Use `make run-disk EFI=on` for the EFI path. The disk image places the bootable GRUB ISO at the beginning and reserves the first 64 MiB for boot media; the remaining blocks are preformatted as the KSFS system volume. Writing the generated raw image to a physical drive replaces that drive's contents.

For installation media that deploys the boot image to a separate hard drive:

```sh
make installer-iso
```

Boot the CD's **Install KyronOS** menu entry, attach the target drive, then type `install` and `install confirm`. The installer copies its validated hybrid BIOS/EFI boot image to the drive and formats the separate KSFS volume. This erases the target drive.

To create CD/update media from a new build, run `make update-iso`. Boot the CD and choose **Update installed KyronOS**, then run `update` and `update confirm` in the shell. The updater patches the reserved kernel file extent in an installed hybrid disk image and leaves its KSFS data region intact.

```sh
gdb build/kyronos.kernel
```

## Shell

The current shell includes:

```text
help clear version about echo apps
ls files cd pwd go back go home go root
touch mkdir rmdir write append cat rm notes images image
settings install update
add user <name>
switch user <name>
devices mem reboot shutdown
```

The desktop dock launches Notes, Files, Settings, Images, and Terminal. Notes are stored under `/home/<user>/Notes`; BMP viewing accepts uncompressed 24-bit and 32-bit files that fit the KSFS file limit. The framebuffer theme uses the GRUB-provided wallpaper, translucent panels, a top menu strip, and a floating dock. Without a Multiboot framebuffer it falls back to VGA text output.

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

The kernel currently supports IDE ATA PIO and SATA AHCI block I/O, a fixed-offset KSFS volume, shell file persistence across reboot, a CD installer that deploys the hybrid GRUB BIOS/EFI boot image, a hybrid disk-image build, PS/2 basic/wheel/five-button mice, USB HID report decoding, BMP viewing, and a CD update path for the reserved boot-kernel slot. Disk installation uses a fixed 64 MiB data offset rather than general partition discovery. Native USB host-controller transport, 64-bit long mode, and direct optical-drive transport remain unfinished.

See [docs/BUILDING.md](docs/BUILDING.md), [docs/WINDOWS.md](docs/WINDOWS.md), [docs/KSFS.md](docs/KSFS.md), [docs/USB.md](docs/USB.md), and [docs/ROADMAP.md](docs/ROADMAP.md) for details.
