CXX ?= g++
AS := nasm
LD ?= ld
GRUB_MKRESCUE ?= $(shell command -v grub2-mkrescue 2>/dev/null || command -v grub-mkrescue 2>/dev/null || echo grub-mkrescue)
GRUB_EFI_DIR ?= /usr/lib/grub/x86_64-efi
VBOXMANAGE ?= VBoxManage
VM_NAME ?= KyronOS
EFI ?= off
VBOX_FIRMWARE := bios
ifeq ($(EFI),on)
VBOX_FIRMWARE := efi
endif
CXXFLAGS := -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -Wall -Wextra -Werror -Iinclude -I.
LDFLAGS := -m elf_i386 -T boot/linker.ld
BUILD := build
DISK_IMAGE := $(BUILD)/kyronos-disk.img
DISK_IMAGE_SIZE := 256M

.PHONY: all kernel iso update-iso installer-iso disk-image run run-disk debug clean test mkfs
all: kernel

kernel: $(BUILD)/kyronos.kernel

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry.o: boot/entry.asm | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/console.o: src/kernel/console.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/graphics.o: graphics/graphics.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/image.o: src/graphics/image.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/keyboard.o: src/kernel/keyboard.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/mouse.o: src/kernel/mouse.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/usb_keyboard.o: src/drivers/usb_keyboard.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/pci.o: src/drivers/pci.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/ahci.o: src/drivers/ahci.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/filesystem.o: src/fs/filesystem.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/ksfs.o: src/fs/ksfs.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/iso9660.o: src/fs/iso9660.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/installer.o: src/fs/installer.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/ata_pio.o: src/drivers/ata_pio.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/shell.o: src/kernel/shell.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/kernel.o: src/kernel/kernel.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/runtime.o: src/kernel/runtime.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/kyronos.kernel: $(BUILD)/entry.o $(BUILD)/console.o $(BUILD)/graphics.o $(BUILD)/image.o $(BUILD)/keyboard.o $(BUILD)/mouse.o $(BUILD)/usb_keyboard.o $(BUILD)/pci.o $(BUILD)/ahci.o $(BUILD)/ata_pio.o $(BUILD)/ksfs.o $(BUILD)/iso9660.o $(BUILD)/filesystem.o $(BUILD)/installer.o $(BUILD)/shell.o $(BUILD)/kernel.o $(BUILD)/runtime.o
	$(LD) $(LDFLAGS) -o $@ $^

iso: kernel
	command -v "$(GRUB_MKRESCUE)" >/dev/null || { echo 'Missing GRUB rescue tool. See docs/BUILDING.md.'; exit 1; }
	test -d "$(GRUB_EFI_DIR)" || { echo 'Missing x86_64 EFI GRUB modules. Install grub-efi-amd64-bin.'; exit 1; }
	mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/kyronos.kernel $(BUILD)/iso/boot/kyronos.kernel
	truncate -s 524288 $(BUILD)/iso/boot/kyronos.kernel
	cp boot/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	cp bg.jpg $(BUILD)/iso/bg.jpg
	$(GRUB_MKRESCUE) -o $(BUILD)/kyronos.iso $(BUILD)/iso

update-iso: iso
	cp $(BUILD)/kyronos.iso $(BUILD)/kyronos-update.iso

installer-iso: iso
	rm -rf $(BUILD)/installer-iso
	mkdir -p $(BUILD)/installer-iso/boot/grub
	cp $(BUILD)/kyronos.kernel $(BUILD)/installer-iso/boot/kyronos.kernel
	truncate -s 524288 $(BUILD)/installer-iso/boot/kyronos.kernel
	cp boot/grub-installer.cfg $(BUILD)/installer-iso/boot/grub/grub.cfg
	cp bg.jpg $(BUILD)/installer-iso/bg.jpg
	cp $(BUILD)/kyronos.iso $(BUILD)/installer-iso/kyronos-disk-boot.bin
	$(GRUB_MKRESCUE) -o $(BUILD)/kyronos-installer.iso $(BUILD)/installer-iso

disk-image: iso tools/ksfs-mkfs
	rm -f $(DISK_IMAGE)
	truncate -s $(DISK_IMAGE_SIZE) $(DISK_IMAGE)
	dd if=$(BUILD)/kyronos.iso of=$(DISK_IMAGE) conv=notrunc status=none
	./tools/ksfs-mkfs $(DISK_IMAGE) 49152 16384

run-disk: disk-image
	$(VBOXMANAGE) showvminfo "$(VM_NAME)" >/dev/null 2>&1 || $(VBOXMANAGE) createvm --name "$(VM_NAME)" --register
	$(VBOXMANAGE) modifyvm "$(VM_NAME)" --firmware $(VBOX_FIRMWARE) --memory 256 --boot1 disk --boot2 dvd --nic1 nat
	$(VBOXMANAGE) storagectl "$(VM_NAME)" --name KyronOSHardDisk --add sata --controller IntelAhci >/dev/null 2>&1 || true
	$(VBOXMANAGE) storageattach "$(VM_NAME)" --storagectl KyronOSHardDisk --port 0 --device 0 --type hdd --medium "$(abspath $(DISK_IMAGE))"
	$(VBOXMANAGE) startvm "$(VM_NAME)" --type gui

run: iso
	$(VBOXMANAGE) showvminfo "$(VM_NAME)" >/dev/null 2>&1 || $(VBOXMANAGE) createvm --name "$(VM_NAME)" --register
	$(VBOXMANAGE) modifyvm "$(VM_NAME)" --firmware $(VBOX_FIRMWARE) --memory 128 --boot1 dvd --boot2 none --nic1 nat
	$(VBOXMANAGE) storagectl "$(VM_NAME)" --name KyronOSStorage --add ide --controller PIIX4 >/dev/null 2>&1 || true
	$(VBOXMANAGE) storageattach "$(VM_NAME)" --storagectl KyronOSStorage --port 0 --device 0 --type dvddrive --medium "$(abspath $(BUILD)/kyronos.iso)"
	$(VBOXMANAGE) startvm "$(VM_NAME)" --type gui

debug: iso
	@echo "VirtualBox debugging requires a configured VirtualBox debug provider."
	@echo "The ISO is ready at $(BUILD)/kyronos.iso."

test:
	$(MAKE) -C tests
	./tests/ksfs_tests
	./tests/image_tests
	./tests/usb_input_tests
	./tests/iso_update_tests

mkfs: tools/ksfs-mkfs
	./tools/ksfs-mkfs $(IMAGE) $(BLOCKS)

tools/ksfs-mkfs: tools/ksfs_mkfs.cpp src/fs/ksfs.cpp src/fs/filesystem.cpp src/fs/installer.cpp
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Iinclude -I. $^ -o $@

clean:
	rm -rf $(BUILD) tests/*.o tests/ksfs_tests tools/ksfs-mkfs
