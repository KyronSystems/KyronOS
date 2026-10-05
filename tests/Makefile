CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -I../include -I..

.PHONY: all clean
all: ksfs_tests image_tests usb_input_tests iso_update_tests

ksfs_tests: ksfs_tests.o fs_ksfs.o fs_filesystem.o fs_installer.o
	$(CXX) $^ -o $@

image_tests: image_tests.o graphics_image.o
	$(CXX) $^ -o $@

usb_input_tests: usb_input_tests.o driver_usb_keyboard.o
	$(CXX) $^ -o $@

iso_update_tests: iso_update_tests.o fs_iso9660.o
	$(CXX) $^ -o $@

ksfs_tests.o: ksfs_tests.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

fs_ksfs.o: ../src/fs/ksfs.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

fs_filesystem.o: ../src/fs/filesystem.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

fs_installer.o: ../src/fs/installer.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

image_tests.o: image_tests.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

graphics_image.o: ../src/graphics/image.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

usb_input_tests.o: usb_input_tests.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

driver_usb_keyboard.o: ../src/drivers/usb_keyboard.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

iso_update_tests.o: iso_update_tests.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

fs_iso9660.o: ../src/fs/iso9660.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o ksfs_tests image_tests usb_input_tests iso_update_tests
