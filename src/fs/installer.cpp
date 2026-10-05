#include "fs/installer.hpp"
#include "fs/filesystem.hpp"

namespace {
uint32_t text_length(const char* text) {
    uint32_t length = 0;
    while (text[length]) ++length;
    return length;
}
}

namespace {
bool add_directory(kyron::fs::FileSystem& filesystem, uint32_t parent, const char* name, uint32_t& inode) {
    return filesystem.create_directory(parent, name, inode);
}

bool add_file(kyron::fs::FileSystem& filesystem, uint32_t parent, const char* name, const char* contents) {
    uint32_t inode = 0;
    return filesystem.create_file(parent, name, contents, text_length(contents), inode);
}
}

namespace kyron::fs {
bool Installer::install(BlockDevice& device, const char* hostname) {
    if (!hostname || !*hostname) return false;
    FileSystem filesystem(device);
    if (!filesystem.format()) return false;
    uint32_t root = KSFS_ROOT_INODE;
    uint32_t kyron = 0;
    uint32_t boot = 0;
    uint32_t system = 0;
    uint32_t etc = 0;
    uint32_t home = 0;
        uint32_t home_user = 0;
        uint32_t notes = 0;
    uint32_t tmp = 0;
    uint32_t dev = 0;
    uint32_t bin = 0;
    uint32_t usr = 0;
    uint32_t usr_bin = 0;
    if (!add_directory(filesystem, root, "kyron", kyron) ||
        !add_directory(filesystem, root, "boot", boot) ||
        !add_directory(filesystem, root, "system", system) ||
        !add_directory(filesystem, root, "etc", etc) ||
        !add_directory(filesystem, root, "home", home) ||
        !add_directory(filesystem, home, "kyron", home_user) ||
        !add_directory(filesystem, home_user, "Notes", notes) ||
        !add_directory(filesystem, root, "tmp", tmp) ||
        !add_directory(filesystem, root, "dev", dev) ||
        !add_directory(filesystem, root, "bin", bin) ||
        !add_directory(filesystem, root, "usr", usr) ||
        !add_directory(filesystem, usr, "bin", usr_bin)) return false;
    if (!add_file(filesystem, root, "README", "KyronOS system root.\n") ||
        !add_file(filesystem, kyron, "README", "KyronOS system files.\n") ||
        !add_file(filesystem, boot, "README", "Boot files are managed by the installer.\n") ||
        !add_file(filesystem, system, "motd", "Welcome to KyronOS.\n") ||
        !add_file(filesystem, etc, "hostname", hostname) ||
        !add_file(filesystem, etc, "version", "KyronOS\n") ||
        !add_file(filesystem, home, "README", "User home directories live here.\n") ||
        !add_file(filesystem, home_user, "Welcome.txt", "Welcome to KyronOS.\nYour files are stored on this disk.\n") ||
        !add_file(filesystem, notes, "Notes.txt", "") ||
        !add_file(filesystem, tmp, "README", "Temporary files live here.\n") ||
        !add_file(filesystem, dev, "README", "Device nodes appear here.\n") ||
        !add_file(filesystem, bin, "README", "Compatibility command links live here.\n") ||
        !add_file(filesystem, usr, "README", "System software and libraries.\n") ||
        !add_file(filesystem, usr_bin, "README", "Installed commands live here.\n")) return false;
    return true;
}
}
