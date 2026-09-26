#!/bin/sh
set -eu
missing=0
for tool in gcc g++ nasm ld make; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf 'found %-18s %s\n' "$tool" "$(command -v "$tool")"
    else
        printf 'missing %s\n' "$tool"
        missing=1
    fi
done
if command -v grub2-mkrescue >/dev/null 2>&1 || command -v grub-mkrescue >/dev/null 2>&1; then
    printf 'found %-18s %s\n' grub-mkrescue "$(command -v grub2-mkrescue 2>/dev/null || command -v grub-mkrescue)"
else
    printf 'missing grub-mkrescue (needed for ISO)\n'
fi
if [ -d /usr/lib/grub/x86_64-efi ]; then
    printf 'found %-18s %s\n' grub-x86_64-efi /usr/lib/grub/x86_64-efi
else
    printf 'missing grub-x86_64-efi (needed for EFI ISO boot)\n'
fi
for tool in mformat xorriso VBoxManage gdb; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf 'found %-18s %s\n' "$tool" "$(command -v "$tool")"
    else
        printf 'missing %s (needed for ISO/VirtualBox/debug)\n' "$tool"
    fi
done
if [ "$missing" -ne 0 ]; then
    cat <<'MSG'

KyronOS needs a freestanding build toolchain.
On Fedora:
        sudo dnf install gcc gcc-c++ make nasm grub2-tools-extra grub2-efi-x64-modules mtools xorriso gdb
On Debian/Ubuntu:
    sudo apt install build-essential nasm grub-pc-bin grub-efi-amd64-bin grub-common mtools xorriso gdb
A dedicated i686-elf cross compiler is recommended for production builds.
Install VirtualBox separately for running the ISO.
No packages were installed by this script.
MSG
    exit 1
fi
printf '\nHost prerequisites are present. Run: make kernel\n'
