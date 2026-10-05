# KSFS format

KSFS is an original, little-endian, fixed-block format. Every block is 4096 bytes. Block 0 is the superblock, block 1 is the free-block bitmap, and blocks beginning at block 2 hold the inode table. Directory and file data follow the inode table.

The superblock contains: magic `0x4B534653` (`KSFS`), version `1`, block size, total blocks, free blocks, inode-table block, inode count, root inode `1`, bitmap block, and an FNV-1a checksum over all fields except checksum. `BlockDevice` supplies full-block reads and writes, keeping KSFS independent of controller hardware.

The implementation formats and validates a superblock, allocates fixed-size inodes and up to twelve direct data blocks per file, stores directory entries, creates and overwrites files, resolves nested paths such as `/etc/hostname` or `projects/todo.txt`, reads contents after remounting, and removes files or empty directories. `fs::Installer` creates the initial root tree including `/home/kyron/Notes`. `BlockDeviceSlice` confines KSFS writes to the volume region beginning at 64 MiB, leaving the hybrid GRUB boot image at the start of the drive. `make installer-iso` builds media that copies the boot image before formatting KSFS; `make disk-image` builds a preformatted BIOS/EFI hybrid drive image.
