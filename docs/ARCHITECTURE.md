# Architecture

KyronOS is a small monolithic kernel for x86 PCs. `boot/` owns the Multiboot2 entry and linker layout. `src/kernel/` owns architecture-neutral kernel services currently present. `include/fs/` defines the storage contract and KSFS metadata. Host-testable code lives in `src/fs/` and `src/shell/`.

The intended boot flow is GRUB -> Multiboot2 entry -> kernel console -> CPU and memory discovery -> block-device registration -> KSFS mount -> terminal shell. Hardware reporting must come from discovered devices; unsupported devices are reported as unavailable.
