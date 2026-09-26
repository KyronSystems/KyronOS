# Windows development

KyronOS can be developed on Windows, but the build and ISO creation process still requires either WSL2 or a virtual machine. The repository's Makefile depends on the GRUB rescue tools (`grub-mkrescue` or `grub2-mkrescue`) and `xorriso`, which are not available directly from a plain MSYS2 installation.

## Install the Windows tools

1. Install [MSYS2](https://www.msys2.org/).
2. Open the **MSYS2 UCRT64** terminal and update its package database:

   ```sh
   pacman -Syu
   ```

   Close and reopen the terminal if MSYS2 asks you to do so, then run the update again.

3. Install the MSYS2 tools available from the UCRT64 environment:

   ```sh
   pacman -S --needed base-devel nasm
   ```

   MSYS2 does not provide the `grub` or `xorriso` packages used to create this
   project's bootable ISO. Do not add those names to the `pacman` command. This
   means the build still requires either WSL2 or a VM with a Linux distro such as
   Debian or Fedora for `make iso`.

4. Install or add an i386 ELF freestanding toolchain and the GRUB rescue tools.
   The kernel Makefile expects:

   - `gcc` and `g++` that accept `-m32` and produce ELF relocatable objects;
   - GNU `ld` that accepts `-m elf_i386`;
   - `nasm` for 32-bit assembly.

   The normal Windows MinGW linker targets Windows PE files and is not a substitute for the ELF linker used by this repository. Keep the i386 ELF toolchain's `bin` directory on `PATH` before the Windows compiler directories.

   You also need `grub-mkrescue` (or `grub2-mkrescue`), the GRUB `x86_64-efi`
   modules, `mformat` from the `mtools` package, and `xorriso` on `PATH` for
   `make iso`. There is no official MSYS2 package for these tools. The reliable
   Windows option is to either use WSL2 for the build or to build the ISO in a
   small Debian or Fedora VirtualBox VM, sharing this repository folder with the
   VM. Source editing and Git can remain on Windows. A prebuilt Windows-compatible
   GRUB rescue bundle is also possible, but it must provide both commands and its
   own GRUB data directory.

## Build from MSYS2

From the MSYS2 terminal, change to the repository directory. For a OneDrive checkout, the path usually looks like this:

```sh
cd /c/Users/<WindowsUser>/OneDrive/Documents/KyronOS
```

Then run:

```sh
./setup.sh
make kernel
make iso
make run
```

The ISO is written to `build/kyronos.iso`. To debug it, use:

```sh
make debug
```

In another MSYS2 terminal, connect GDB to a separately configured VirtualBox debug provider:

```sh
gdb build/kyronos.kernel
(gdb) target remote :1234
(gdb) continue
```

## Host tests

The KSFS tests are ordinary C++ host tests and can be built from the same MSYS2 terminal:

```sh
make test
```

## PowerShell note

PowerShell is fine for editing the source and running Git commands. Use the MSYS2 terminal for `setup.sh`, `make`, and the POSIX build commands in the Makefile. Install VirtualBox separately, ensure `VBoxManage` is on the MSYS2 `PATH`, and run `make run` or `make run EFI=on` from MSYS2 after the ISO toolchain is available. If you do not have WSL2, then use a Debian or Fedora VirtualBox build VM for `make iso` and `make run`.
