#include "kernel/shell.hpp"
#include "kernel/console.hpp"
#include "kernel/keyboard.hpp"
#include <stdint.h>

namespace {
constexpr uint32_t max_entries = 32;
constexpr uint32_t name_size = 32;
constexpr uint32_t data_size = 256;
constexpr uint32_t max_users = 8;
struct Entry { char name[name_size]; char data[data_size]; uint32_t parent; bool directory; bool used; };
struct User { char name[name_size]; uint32_t home; bool used; };
Entry entries[max_entries];
User users[max_users];
uint32_t home_directory = 0;
uint32_t current_user = 0;
uint32_t current_directory = 0;
uint32_t previous_directory = 0;
char cwd[64] = "/";
const char hostname[] = "kyron";

uint32_t length(const char* text) { uint32_t size = 0; while (text[size]) ++size; return size; }
bool equals(const char* left, const char* right) {
    uint32_t index = 0; while (left[index] || right[index]) { if (left[index] != right[index]) return false; ++index; } return true;
}
void copy(char* destination, const char* source, uint32_t capacity) {
    uint32_t index = 0; while (index + 1 < capacity && source[index]) { destination[index] = source[index]; ++index; } destination[index] = 0;
}
void build_path(uint32_t directory, char* destination, uint32_t capacity) {
    if (directory == 0) { copy(destination, "/", capacity); return; }
    uint32_t stack[8]; uint32_t depth = 0; uint32_t current = directory;
    while (current != 0 && depth < 8) { stack[depth++] = current; current = entries[current].parent; }
    destination[0] = 0;
    for (uint32_t index = depth; index > 0; --index) {
        uint32_t offset = length(destination);
        if (offset + 1 < capacity) destination[offset++] = '/';
        copy(destination + offset, entries[stack[index - 1]].name, capacity - offset);
    }
}
void refresh_path() { build_path(current_directory, cwd, sizeof(cwd)); }
const char* argument(char* line) { while (*line && *line != ' ') ++line; while (*line == ' ') ++line; return line; }
Entry* find(const char* name, uint32_t parent) { for (uint32_t i = 0; i < max_entries; ++i) if (entries[i].used && entries[i].parent == parent && equals(entries[i].name, name)) return &entries[i]; return nullptr; }
uint32_t directory_for_path(const char* path, uint32_t starting_directory) {
    uint32_t directory = (*path == '/') ? 0 : starting_directory;
    if (*path == '/') ++path;
    while (*path) {
        char component[name_size];
        uint32_t index = 0;
        while (path[index] && path[index] != '/' && index + 1 < name_size) { component[index] = path[index]; ++index; }
        component[index] = 0;
        while (path[index] == '/') ++index;
        path += index;
        if (!*component || equals(component, ".")) continue;
        if (equals(component, "..")) { if (directory != 0) directory = entries[directory].parent; continue; }
        Entry* entry = find(component, directory);
        if (!entry || !entry->directory) return max_entries;
        directory = static_cast<uint32_t>(entry - entries);
    }
    return directory;
}
bool split_parent_and_name(const char* path, uint32_t starting_directory, uint32_t& parent, char* name, uint32_t capacity) {
    if (!path || !*path || !name || capacity == 0) return false;
    char buffer[128];
    uint32_t index = 0;
    while (path[index] && index + 1 < sizeof(buffer)) { buffer[index] = path[index]; ++index; }
    buffer[index] = 0;
    if (buffer[0] == '/' && buffer[1] == 0) return false;
    uint32_t last_slash = 0;
    for (uint32_t i = 0; buffer[i]; ++i) if (buffer[i] == '/') last_slash = i;
    if (buffer[0] == '/' && last_slash == 0) {
        parent = 0;
        copy(name, buffer + 1, capacity);
        return *name != 0;
    }
    if (last_slash == 0) {
        parent = starting_directory;
        copy(name, buffer, capacity);
        return true;
    }
    char parent_path[128];
    for (uint32_t i = 0; i < last_slash; ++i) parent_path[i] = buffer[i];
    parent_path[last_slash] = 0;
    parent = directory_for_path(parent_path, starting_directory);
    if (parent == max_entries) return false;
    copy(name, buffer + last_slash + 1, capacity);
    return *name != 0;
}
Entry* resolve_entry_path(const char* path, uint32_t starting_directory) {
    uint32_t parent = 0;
    char name_buffer[name_size];
    if (!split_parent_and_name(path, starting_directory, parent, name_buffer, sizeof(name_buffer))) return nullptr;
    if (parent == max_entries) return nullptr;
    return find(name_buffer, parent);
}
Entry* create(const char* name, bool directory, uint32_t parent) {
    if (!*name || length(name) >= name_size || find(name, parent)) return nullptr;
    for (uint32_t i = 0; i < max_entries; ++i) if (!entries[i].used) { entries[i].used = true; entries[i].directory = directory; entries[i].parent = parent; copy(entries[i].name, name, name_size); return &entries[i]; }
    return nullptr;
}
Entry* create_file(const char* name, uint32_t parent, const char* contents) {
    Entry* entry = create(name, false, parent);
    if (!entry) return nullptr;
    copy(entry->data, contents, data_size);
    return entry;
}
void add_user(const char* name) {
    if (!*name || length(name) >= name_size) { console::write_line("Invalid username.", 0x0C); return; }
    for (uint32_t i = 0; i < max_users; ++i) if (users[i].used && equals(users[i].name, name)) { console::write_line("User already exists.", 0x0C); return; }
    for (uint32_t i = 0; i < max_users; ++i) if (!users[i].used) {
        Entry* home = create(name, true, home_directory);
        if (!home) { console::write_line("Unable to create home folder.", 0x0C); return; }
        users[i].used = true; copy(users[i].name, name, name_size); users[i].home = static_cast<uint32_t>(home - entries);
        console::write_line("User added.", 0x0B); return;
    }
    console::write_line("User limit reached.", 0x0C);
}
void switch_user(const char* name) {
    for (uint32_t i = 0; i < max_users; ++i) if (users[i].used && equals(users[i].name, name)) {
        previous_directory = current_directory; current_user = i; current_directory = users[i].home; refresh_path(); return;
    }
    console::write_line("User not found.", 0x0C);
}
void write_prompt_path() {
    console::write("[", 0x0D);
    console::write(users[current_user].name, 0x0D);
    console::write("@", 0x0D);
    console::write(hostname, 0x0D);
    console::write("]{", 0x0D);
    console::write(cwd, 0x0D);
    console::write("}", 0x0D);
}
void help() {
    console::write_line("Commands:", 0x0D);
    console::write_line("  help [command]       show command help", 0x0B);
    console::write_line("  clear                clear the screen", 0x0B);
    console::write_line("  version about        show KyronOS version", 0x0B);
    console::write_line("  echo <text>          print text", 0x0B);
    console::write_line("  pwd                  print current directory", 0x0B);
    console::write_line("  ls [directory]       list a directory", 0x0B);
    console::write_line("  cd <directory>       change directory", 0x0B);
    console::write_line("  go back|home|root    change to a known directory", 0x0B);
    console::write_line("  touch <file>         create a file", 0x0B);
    console::write_line("  mkdir <directory>    create a directory", 0x0B);
    console::write_line("  rmdir <directory>    remove a directory", 0x0B);
    console::write_line("  write <file> <text>   replace file contents", 0x0B);
    console::write_line("  append <file> <text>  append to a file", 0x0B);
    console::write_line("  cat <file>           print a file", 0x0B);
    console::write_line("  rm <file>            remove a file", 0x0B);
    console::write_line("  devices mem reboot shutdown", 0x0B);
}
void command_help(const char* name) {
    if (equals(name, "ls")) console::write_line("ls [directory] - list entries in a directory.", 0x0B);
    else if (equals(name, "cd")) console::write_line("cd <directory> - change to a directory.", 0x0B);
    else if (equals(name, "pwd")) console::write_line("pwd - print the current directory.", 0x0B);
    else if (equals(name, "touch")) console::write_line("touch <file> - create an empty file.", 0x0B);
    else if (equals(name, "mkdir")) console::write_line("mkdir <directory> - create a directory.", 0x0B);
    else if (equals(name, "rmdir")) console::write_line("rmdir <directory> - remove a directory.", 0x0B);
    else if (equals(name, "write")) console::write_line("write <file> <text> - replace file contents.", 0x0B);
    else if (equals(name, "append")) console::write_line("append <file> <text> - append to a file.", 0x0B);
    else if (equals(name, "cat")) console::write_line("cat <file> - print file contents.", 0x0B);
    else if (equals(name, "rm")) console::write_line("rm <file> - remove a file.", 0x0B);
    else if (equals(name, "echo")) console::write_line("echo <text> - print text.", 0x0B);
    else if (equals(name, "help")) console::write_line("help [command] - list commands or show command help.", 0x0B);
    else if (equals(name, "clear")) console::write_line("clear - clear the console.", 0x0B);
    else if (equals(name, "version") || equals(name, "about")) console::write_line("version/about - show KyronOS version.", 0x0B);
    else if (equals(name, "devices")) console::write_line("devices - list detected hardware.", 0x0B);
    else if (equals(name, "mem")) console::write_line("mem - show memory information.", 0x0B);
    else if (equals(name, "reboot")) console::write_line("reboot - restart the machine.", 0x0B);
    else if (equals(name, "shutdown")) console::write_line("shutdown - halt the machine.", 0x0B);
    else console::write_line("No help is available for that command.", 0x0C);
}
void list(uint32_t directory) {
    for (uint32_t i = 0; i < max_entries; ++i) if (entries[i].used && entries[i].parent == directory) { console::write(entries[i].name); console::write_line(entries[i].directory ? "/" : ""); }
}
void change_directory(const char* target) {
    if (!target || !*target) { console::write_line("Directory not found.", 0x0C); return; }
    uint32_t old_directory = current_directory;
    uint32_t next_directory = directory_for_path(target, current_directory);
    if (next_directory == max_entries || !entries[next_directory].directory) {
        console::write_line("Directory not found.", 0x0C); return;
    }
    current_directory = next_directory;
    previous_directory = old_directory;
    refresh_path();
}
void go_back() { uint32_t swap = current_directory; current_directory = previous_directory; previous_directory = swap; refresh_path(); }
void go_home() { previous_directory = current_directory; current_directory = users[current_user].home; refresh_path(); }
void go_root() { previous_directory = current_directory; current_directory = 0; refresh_path(); }
void remove_entry(const char* path) {
    Entry* entry = resolve_entry_path(path, current_directory);
    if (!entry) { console::write_line("File not found.", 0x0C); return; }
    entry->used = false;
}
void write_file(const char* command, bool append) {
    const char* name = command; while (*name == ' ') ++name;
    char file_name[name_size]; uint32_t index = 0; while (name[index] && name[index] != ' ' && index + 1 < name_size) { file_name[index] = name[index]; ++index; } file_name[index] = 0;
    const char* text = name + index; while (*text == ' ') ++text;
    uint32_t parent = 0;
    char target_name[name_size];
    if (!split_parent_and_name(file_name, current_directory, parent, target_name, sizeof(target_name))) {
        console::write_line("Unable to write file.", 0x0C); return;
    }
    Entry* entry = find(target_name, parent);
    if (!entry && !append) entry = create(target_name, false, parent);
    if (!entry || entry->directory) { console::write_line("Unable to write file.", 0x0C); return; }
    uint32_t offset = append ? length(entry->data) : 0; uint32_t available = data_size - offset;
    for (uint32_t i = 0; i + 1 < available && text[i]; ++i) entry->data[offset + i] = text[i];
    entry->data[data_size - 1] = 0; console::write_line("OK", 0x0B);
}
uint32_t complete(char* buffer, uint32_t input_length, uint32_t capacity) {
    uint32_t start = input_length;
    while (start > 0 && buffer[start - 1] != ' ') --start;
    uint32_t prefix_length = input_length - start;
    uint32_t slash = prefix_length;
    while (slash > 0 && buffer[start + slash - 1] != '/') --slash;
    char directory_path[name_size];
    for (uint32_t i = 0; i < slash; ++i) directory_path[i] = buffer[start + i];
    directory_path[slash] = 0;
    uint32_t directory = directory_for_path(directory_path, current_directory);
    if (directory == max_entries) return input_length;
    char prefix[name_size];
    uint32_t name_prefix_length = prefix_length - slash;
    for (uint32_t i = 0; i < name_prefix_length && i + 1 < name_size; ++i) prefix[i] = buffer[start + slash + i];
    prefix[name_prefix_length < name_size ? name_prefix_length : name_size - 1] = 0;
    char common[name_size]; uint32_t common_length = 0; uint32_t match_count = 0; bool found = false;
    Entry* match = nullptr;
    for (uint32_t i = 0; i < max_entries; ++i) if (entries[i].used && entries[i].parent == directory) {
        bool matches = true; for (uint32_t j = 0; j < name_prefix_length; ++j) if (entries[i].name[j] != prefix[j]) matches = false;
        if (!matches) continue;
        match = &entries[i];
        ++match_count;
        if (!found) { copy(common, entries[i].name, name_size); common_length = length(common); found = true; }
        else { uint32_t j = 0; while (j < common_length && entries[i].name[j] == common[j]) ++j; common_length = j; common[j] = 0; }
    }
    if (!found || (common_length <= name_prefix_length && !(match_count == 1 && common_length == name_prefix_length && match->directory))) return input_length;
    uint32_t new_length = input_length;
    while (new_length < start + slash + common_length && new_length + 1 < capacity) {
        buffer[new_length] = common[new_length - start - slash];
        ++new_length;
    }
    if (match_count == 1 && match && match->directory && new_length == start + slash + common_length && new_length + 1 < capacity) buffer[new_length++] = '/';
    return new_length;
}
void command(char* line) {
    if (!*line) return;
    uint32_t line_length = length(line);
    if (line_length > 5 && line[line_length - 5] == ' ' && equals(line + line_length - 4, "help")) {
        line[line_length - 5] = 0;
        command_help(line);
        return;
    }
    if (equals(line, "help")) help();
    else if (equals(line, "clear")) console::clear();
    else if (equals(line, "version") || equals(line, "about")) console::write_line("KyronOS Alpha 2", 0x0D);
    else if (equals(line, "go back")) go_back();
    else if (equals(line, "go home")) go_home();
    else if (equals(line, "go root")) go_root();
    else if (line[0] == 'a' && line[1] == 'd' && line[2] == 'd' && line[3] == ' ' && line[4] == 'u' && line[5] == 's' && line[6] == 'e' && line[7] == 'r' && line[8] == ' ') add_user(line + 9);
    else if (line[0] == 's' && line[1] == 'w' && line[2] == 'i' && line[3] == 't' && line[4] == 'c' && line[5] == 'h' && line[6] == ' ' && line[7] == 'u' && line[8] == 's' && line[9] == 'e' && line[10] == 'r' && line[11] == ' ') switch_user(line + 12);
    else if (line[0] == 'c' && line[1] == 'd' && line[2] == ' ') change_directory(argument(line));
    else if (equals(line, "pwd")) console::write_line(cwd, 0x0B);
    else if (equals(line, "ls")) list(current_directory);
    else if (equals(line, "devices")) console::write_line("CPU: x86 | Keyboard: PS/2 | Storage: RAM KSFS", 0x0B);
    else if (equals(line, "mem")) console::write_line("Memory information unavailable in Alpha 2 bootstrap.", 0x0D);
    else if (equals(line, "reboot")) { asm volatile("cli; hlt"); }
    else if (equals(line, "shutdown")) { asm volatile("cli; hlt"); }
    else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' && line[3] == 'o' && line[4] == ' ') console::write_line(argument(line));
    else if (line[0] == 'c' && line[1] == 'a' && line[2] == 't' && line[3] == ' ') {
        Entry* entry = resolve_entry_path(argument(line), current_directory);
        if (entry && !entry->directory) console::write_line(entry->data); else console::write_line("File not found.", 0x0C);
    }
    else if (line[0] == 'l' && line[1] == 's' && line[2] == ' ') {
        uint32_t directory = directory_for_path(argument(line), current_directory);
        if (directory == max_entries) console::write_line("Directory not found.", 0x0C); else list(directory);
    }
    else if (line[0] == 't' && line[1] == 'o' && line[2] == 'u' && line[3] == 'c' && line[4] == 'h' && line[5] == ' ') {
        char file_name[name_size];
        uint32_t parent = 0;
        const char* target = argument(line);
        if (!split_parent_and_name(target, current_directory, parent, file_name, sizeof(file_name))) {
            console::write_line("Unable to create file.", 0x0B);
        } else {
            console::write_line(create(file_name, false, parent) ? "OK" : "Unable to create file.", 0x0B);
        }
    }
    else if (line[0] == 'm' && line[1] == 'k' && line[2] == 'd' && line[3] == 'i' && line[4] == 'r' && line[5] == ' ') {
        char file_name[name_size];
        uint32_t parent = 0;
        const char* target = argument(line);
        if (!split_parent_and_name(target, current_directory, parent, file_name, sizeof(file_name))) {
            console::write_line("Unable to create directory.", 0x0B);
        } else {
            console::write_line(create(file_name, true, parent) ? "OK" : "Unable to create directory.", 0x0B);
        }
    }
    else if (line[0] == 'r' && line[1] == 'm' && line[2] == ' ') remove_entry(argument(line));
    else if (line[0] == 'r' && line[1] == 'm' && line[2] == 'd' && line[3] == 'i' && line[4] == 'r' && line[5] == ' ') remove_entry(argument(line));
    else if (line[0] == 'w' && line[1] == 'r' && line[2] == 'i' && line[3] == 't' && line[4] == 'e' && line[5] == ' ') write_file(argument(line), false);
    else if (line[0] == 'a' && line[1] == 'p' && line[2] == 'p' && line[3] == 'e' && line[4] == 'n' && line[5] == 'd' && line[6] == ' ') write_file(argument(line), true);
    else console::write_line("Command not found.", 0x0C);
}
}

namespace shell {
[[noreturn]] void run() {
    entries[0].used = true; entries[0].directory = true; entries[0].parent = 0; copy(entries[0].name, "/", name_size);
    Entry* kyron_root = create("kyron", true, 0);
    Entry* boot = create("boot", true, 0);
    Entry* system = create("system", true, 0);
    Entry* etc = create("etc", true, 0);
    Entry* home = create("home", true, 0);
    home_directory = static_cast<uint32_t>(home - entries);
    Entry* kyron_home = create("kyron", true, home_directory);
    users[0].used = true; users[0].home = static_cast<uint32_t>(kyron_home - entries); copy(users[0].name, "kyron", name_size);
    Entry* tmp = create("tmp", true, 0);
    Entry* dev = create("dev", true, 0);
    create("bin", true, 0);
    Entry* usr = create("usr", true, 0);
    Entry* usr_bin = create("bin", true, static_cast<uint32_t>(usr - entries));
    create_file("README", static_cast<uint32_t>(kyron_root - entries), "KyronOS system root.\n");
    create_file("README", static_cast<uint32_t>(usr - entries), "System software and libraries.\n");
    create_file("README", static_cast<uint32_t>(usr_bin - entries), "Installed commands live here.\n");
    create_file("motd", static_cast<uint32_t>(system - entries), "Welcome to KyronOS Alpha 2.\n");
    create_file("hostname", static_cast<uint32_t>(etc - entries), hostname);
    create_file("README", static_cast<uint32_t>(boot - entries), "Boot files are managed by the installer.\n");
    create_file("README", static_cast<uint32_t>(dev - entries), "Device nodes appear here.\n");
    create_file("README", static_cast<uint32_t>(tmp - entries), "Temporary files live here.\n");
    create_file("README", 0, "KyronOS Alpha 2 system root.\n");
    create_file("help", static_cast<uint32_t>(usr_bin - entries), "#!/kyron-shell\n# Edit this command description.\nhelp [command]\n");
    create_file("ls", static_cast<uint32_t>(usr_bin - entries), "#!/kyron-shell\n# List directory entries.\n");
    create_file("cd", static_cast<uint32_t>(usr_bin - entries), "#!/kyron-shell\n# Change the current directory.\n");
    create_file("cat", static_cast<uint32_t>(usr_bin - entries), "#!/kyron-shell\n# Print a text file.\n");
    create_file("echo", static_cast<uint32_t>(usr_bin - entries), "#!/kyron-shell\n# Print text to the console.\n");
    create_file("touch", static_cast<uint32_t>(usr_bin - entries), "#!/kyron-shell\n# Create an empty file.\n");
    create_file("mkdir", static_cast<uint32_t>(usr_bin - entries), "#!/kyron-shell\n# Create a directory.\n");
    create_file("rm", static_cast<uint32_t>(usr_bin - entries), "#!/kyron-shell\n# Remove a file.\n");
    create_file("kernel.cpp", static_cast<uint32_t>(system - entries), "// KyronOS kernel source notes.\n// Edit system services here when source editing is available.\n");
    create_file("shell.cpp", static_cast<uint32_t>(system - entries), "// KyronOS shell source notes.\n// Built-in command implementations live in the kernel.\n");
    create_file("shell.conf", static_cast<uint32_t>(etc - entries), "hostname=kyron\nroot_mode=ram\n");
    current_directory = static_cast<uint32_t>(kyron_home - entries);
    refresh_path();
    char line[128];
    console::write_line("Welcome to KyronOS Alpha 2.", 0x0F);
    console::write_line("Type 'help' for available commands.", 0x0B);
    while (true) {
        write_prompt_path();
        console::write("# ", 0x0D);
        keyboard::read_line(line, sizeof(line), complete);
        command(line);
    }
}
}
