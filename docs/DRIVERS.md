# Drivers

KyronOS uses a generic device/block-device boundary. PCI configuration-space scanning and polling IDE/ATA PIO support are implemented first; the IDE backend exposes 4 KiB blocks to KSFS through 28-bit LBA.

SATA/AHCI, NVMe, SCSI, eMMC, SD, USB mass-storage, partition discovery, and interrupt/DMA transport are still pending. Future controller drivers can provide a `BlockDevice` without changing KSFS or the installer.
